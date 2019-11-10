#include "MainComponent.h"

using namespace std;

MainComponent::MainComponent()	: forwardFFT(fftOrder)
								, m_audioSetup(deviceManager)
{
    setSize (1700, 600);
	setAudioChannels(2, 0);
	startTimerHz(40);
	LISAsender.connect("127.0.0.1", 8880);
    REAPERsender.connect("127.0.0.1", 9990);

	sendOSCtoReaper(0); // Stop
	sendOSCtoReaper(2); // Sets the loop start and end points.

	openAudioDeviceManager.setButtonText("Setup");
	openAudioDeviceManager.addListener(this);
	addAndMakeVisible(&openAudioDeviceManager);
	// load settings file if available
	String filePath = File::getSpecialLocation(File::SpecialLocationType::currentApplicationFile).getParentDirectory().getFullPathName();
	audioSettingsFile = File(filePath + "/" + "SALTEAudioSettings.conf");

	//if (audioSettingsFile.existsAsFile())
	//	loadSettings();

	// GUI COLORS
	LookAndFeel& lookAndFeel = getLookAndFeel();
	lookAndFeel.setColour(TextButton::buttonColourId, Colour(0, 0, 0));

}

MainComponent::~MainComponent()
{
	saveSettings();
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	currentSampleRate = sampleRate;
	currentBlockSize = samplesPerBlockExpected;
	currentFFTSize = fftSize;
}

void MainComponent::getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill)
{
	if (bufferToFill.buffer->getNumChannels() > 0)
	{
		auto* channelData = bufferToFill.buffer->getReadPointer(1, bufferToFill.startSample);
		for (auto i = 0; i < bufferToFill.numSamples; ++i)
			pushNextSampleIntoFifo(channelData[i]);
	}
	
	bufferToFill.clearActiveBufferRegion();
}

void MainComponent::releaseResources()
{

}

// Main callback
void MainComponent::timerCallback()
{
	if (nextFFTBlockReady)
	{
		int scalingBin = 400;
		forwardFFT.performFrequencyOnlyForwardTransform(fftData);
		FloatVectorOperations::copy(fftDataCopy, fftData, fftSize / 2);
		FloatVectorOperations::copy(fftDataCopyScaled, fftData, fftSize / 2);
		FloatVectorOperations::multiply(fftDataCopyScaled, 1 / fftDataCopy[scalingBin], fftSize / 2);

		int binNums[16] = {271, 283, 290, 296, 307, 311, 321, 328, 338, 346, 351, 361, 369, 378, 386, 394 };
		// 0-4 -> Distance
		// 5-9 -> Azimuth
		// 10 -> Button Controls


		if (fftDataCopyScaled[394] > 0.995 && fftDataCopyScaled[394] < 1.005 && fftDataCopy[scalingBin] > 40)
		{
			// Loops in the Distance and azimuth bins
			for (int i = 0; i < 5; ++i)
			{
				distance[i] = fftDataCopyScaled[binNums[i]];

			}

			for (int i = 5; i < 10; ++i)
			{
				azimuth[i - 5] = fftDataCopyScaled[binNums[i]];
			}

			sendOSCtoLISA();

			// Control messages
			control = fftDataCopyScaled[binNums[10]];

			// Sends play message to Reaper
			if (control > 0.5f && reaperStopped)
			{
				sendOSCtoReaper(1);
				reaperStopped = false;
			}
			else if(control <= 0.5f && reaperStopped == false)
			{
				sendOSCtoReaper(0);
				reaperStopped = true;
			}
		}

		repaint();
		nextFFTBlockReady = false;
	}
}


// It sends OSC messages to L-ISA Controller
void MainComponent::sendOSCtoLISA()
{
    int sources = 5; // Number of sources

    for (int i = 1; i <= sources; ++i)
    {
        // Defines the tresholds to send or not a message
        //float upDistTresh = lastDistance[i]*1.05;
        //float upAzTresh = lastDistance[i]*1.05;
        //float downDistTresh = lastDistance[i]*0.95;
        //float downAzTresh = lastDistance[i]*0.95;
        
        int channel = i*2 -1; // L-ISA Controller works in pairs (stereo tracks).
        
        // Only sends OSC messages if the new value is bigger or smaller than the treshold.
        // if (lastDistance[i] > upDistTresh || lastDistance[i] < downDistTresh)
		if (true)
        {
            // lastDistance[i] = distance[i];
            String address = "/ext/src/" + (String)channel + "/d";
            LISAsender.send(address, (float)distance[i]);
        }
        /*if (lastAzimuth[i] > upAzTresh || lastAzimuth[i] < downAzTresh)*/
		if (true)
        {
            // lastAzimuth[i] = azimuth[i];
            String address = "/ext/src/" + (String)channel + "/p";
			LISAsender.send(address, (float)azimuth[i]);
        }
    }
}


// It sends OSC control messages to Reaper
void MainComponent::sendOSCtoReaper(int buttonNum)
{
    switch (buttonNum) {
        case 0:
            REAPERsender.send("/stop", (float)1.0);
            break;
        case 1:
            REAPERsender.send("/play", (float)1.0);
            break;
        case 2:
            REAPERsender.send("/loop/start/time", (float)33.103);
            REAPERsender.send("/loop/end/time", (float)77.241);
            REAPERsender.send("/time", (float)33.103);
            break;
            
        default:
            printf("Undefined button number.");
            break;
    }
}



void MainComponent::pushNextSampleIntoFifo(float sample)
{
	// if the fifo contains enough data, set a flag to say
	// that the next line should now be rendered..
	if (fifoIndex == fftSize)    // [8]
	{
		if (!nextFFTBlockReady) // [9]
		{
			zeromem(fftData, sizeof(fftData));
			memcpy(fftData, fifo, sizeof(fifo));
			nextFFTBlockReady = true;
		}
		fifoIndex = 0;
	}
	fifo[fifoIndex++] = sample;  // [9]
}


void MainComponent::paint (Graphics& g)
{
    g.fillAll(Colours::black);

	int startX = 20;
	int startY = 500;
	int scaleX = 8;
	int scaleY = 6;
	int startBin = 256;
	int zoomLength = fftSize / 2 - startBin;

	for (int i = 0; i < zoomLength; ++i)
	{
		g.setColour(Colours::orange);
		int j = startBin + i;
		g.drawLine(startX + i * scaleX, startY - fftDataCopy[j] * scaleY, startX + (i + 1) * scaleX, startY - fftDataCopy[j + 1] * scaleY, 1.0f);
		
		if (fftDataCopy[j] > 1.5f)
		{
			int circleRadius = 3;
			g.drawEllipse(startX + i * scaleX - circleRadius, startY - fftDataCopy[j] * scaleY - circleRadius, 2 * circleRadius, 2 * circleRadius, 1.5f);
			g.drawText(String(j), startX + i * scaleX - 20, startY + 10, 40, 20, Justification::centred);
			g.drawText(String(fftDataCopy[j],2), startX + i * scaleX - 20, startY - fftDataCopy[j] * scaleY - 25, 40, 20, Justification::centred);
			g.setColour(Colours::red);
			g.drawText(String(fftDataCopyScaled[j], 4), startX + i * scaleX - 20, startY - fftDataCopy[j] * scaleY - 40, 40, 20, Justification::centred);
		}
	}

	g.setColour(Colours::white);
	g.drawText("Sample rate: " + String(currentSampleRate), 130, getHeight() - 70, 120, 20, Justification::centredLeft);
	g.drawText("Buffer size: " + String(currentBlockSize), 130, getHeight() - 50, 120, 20, Justification::centredLeft);
	g.drawText("FFT size: " + String(currentFFTSize), 130, getHeight() - 30, 120, 20, Justification::centredLeft);
}

void MainComponent::resized()
{
	m_audioSetup.setCentrePosition(getWidth() / 2, getHeight() / 2);
	openAudioDeviceManager.setBounds(10, getHeight() - 35, 100, 25);
}

void MainComponent::buttonClicked(Button* buttonThatWasClicked)
{
	if (buttonThatWasClicked == &openAudioDeviceManager)
	{
		addAndMakeVisible(m_audioSetup);
		m_audioSetup.m_shouldBeVisible = true;
	}
}

void MainComponent::loadSettings()
{
	XmlDocument asxmldoc(audioSettingsFile);
	std::unique_ptr<XmlElement> audioDeviceSettings(asxmldoc.getDocumentElement());
	deviceManager.initialise(0, 2, audioDeviceSettings.get(), true);
}

void MainComponent::saveSettings()
{
	std::unique_ptr<XmlElement> audioDeviceSettings(deviceManager.createStateXml());
	if (audioDeviceSettings.get())
	{
		audioDeviceSettings->writeTo(audioSettingsFile);
	}
}