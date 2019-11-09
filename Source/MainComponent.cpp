#include "MainComponent.h"

MainComponent::MainComponent()	: forwardFFT(fftOrder)
								, m_audioSetup(deviceManager)
{
    setSize (1000, 600);
	setAudioChannels(2, 0);
	startTimerHz(75);
	LISAsender.connect("10.10.54.75", 8880);
    REAPERsender.connect("10.10.54.75", 9990);

	openAudioDeviceManager.setButtonText("Setup");
	openAudioDeviceManager.addListener(this);
	addAndMakeVisible(&openAudioDeviceManager);
	// load settings file if available
	String filePath = File::getSpecialLocation(File::SpecialLocationType::currentApplicationFile).getParentDirectory().getFullPathName();
	audioSettingsFile = File(filePath + "/" + "SALTEAudioSettings.conf");

	if (audioSettingsFile.existsAsFile())
		loadSettings();

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
    sendOSCtoReaper(2); // Sets the loop start and end points.
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

/*void MainComponent::timerCallback()
{
    if (nextFFTBlockReady)
    {
        int scalingBin = 396;
        forwardFFT.performFrequencyOnlyForwardTransform(fftData);
        FloatVectorOperations::copy(fftDataCopy, fftData, fftSize / 2);
        FloatVectorOperations::copy(fftDataCopyScaled, fftData, fftSize / 2);
        FloatVectorOperations::multiply(fftDataCopyScaled, 1 / fftDataCopy[scalingBin], fftSize / 2);

        float RollOUT = -jmap(fftDataCopyScaled[316], (float)0, (float)1, (float)-180, (float)180);
        float PitchOUT = -jmap(fftDataCopyScaled[305], (float)0, (float)1, (float)-180, (float)180);
        float YawOUT = jmap(fftDataCopyScaled[310], (float)0, (float)1, (float)-180, (float)180);

        if (fftDataCopy[scalingBin] > 5 && fftDataCopyScaled[391] > 0.5)
        {
            sender.send("/rendering/htrpy", RollOUT, PitchOUT, YawOUT);

            // Map and send OSC
            float RollOSC = (float)jmap(-RollOUT, (float)-180, (float)180, (float)0, (float)1);
            float PitchOSC = (float)jmap(-PitchOUT, (float)-180, (float)180, (float)0, (float)1);
            float YawOSC = (float)jmap(-YawOUT, (float)-180, (float)180, (float)0, (float)1);
            sender.send("/roll/", (float)RollOSC);
            sender.send("/pitch/", (float)PitchOSC);
            sender.send("/yaw/", (float)YawOSC);
        }

        //sender.send("/levels/", (float)fftData[32], (float)fftData[40], (float)fftData[48],
        //                        (float)fftData[56], (float)fftData[64], (float)fftData[72],
        //                        (float)fftData[80], (float)fftData[88], (float)fftData[96],
        //                        (float)fftData[104], (float)fftData[112], (float)fftData[120]);

        repaint();
        nextFFTBlockReady = false;
    }
    
    sendOSCtoLISA();
}*/


// Main callback
void MainComponent::timerCallback()
{
    int scalingBin = 396;
    
    forwardFFT.performFrequencyOnlyForwardTransform(fftData);
    FloatVectorOperations::copy(fftDataCopy, fftData, fftSize / 2);
    FloatVectorOperations::copy(fftDataCopyScaled, fftData, fftSize / 2);
    FloatVectorOperations::multiply(fftDataCopyScaled, 1 / fftDataCopy[scalingBin], fftSize / 2);
    
    int binNums [12] = {0,1,2,3,4,5,6,7,8,9,10,11};
    // 0 -> Button Controls
    // 1-5 -> Distance
    // 6-10 -> Azimuth
    // 11 -> Reference level
    
    // Gets the reference level
    referenceLevel = -jmap(fftDataCopyScaled[binNums[11]], (float)0, (float)1, (float)-180, (float)180);
    
    // Loops in the Distance and azimuth bins
    for (int i = 1; i<=5; ++i) {
        distance[i-1] = -jmap(fftDataCopyScaled[binNums[i]], (float)0, (float)1, (float)0, (float)referenceLevel);
    }
    for (int i = 6; i<=10; ++i) {
        azimuth[i-6] = -jmap(fftDataCopyScaled[binNums[i]], (float)0, (float)1, (float)0, (float)referenceLevel);
    }
    sendOSCtoLISA();
    
    // Control messages
    control = -jmap(fftDataCopyScaled[binNums[0]], (float)0, (float)1, (float)0, (float)referenceLevel);
    
    // Sends play message to Reaper
    if (control > lastControl+referenceLevel/2)
    {
        lastControl = control;
        sendOSCtoReaper(1);
    }
    
    // Sends stop message to Reaper
    if (control < lastControl-referenceLevel/2)
    {
        lastControl = control;
        sendOSCtoReaper(0);
    }
}


// It sends OSC messages to L-ISA Controller
void MainComponent::sendOSCtoLISA()
{
    int sources = 5; // Number of sources
    
    for (int i = 1; i <= sources; ++i)
    {
        // Defines the tresholds to send or not a message
        float upDistTresh = lastDistance[i]*1.05;
        float upAzTresh = lastDistance[i]*1.05;
        float downDistTresh = lastDistance[i]*0.95;
        float downAzTresh = lastDistance[i]*0.95;
        
        int channel = i*2 -1; // L-ISA Controller works in pairs (stereo tracks).
        
        // Only sends OSC messages if the new value is bigger or smaller than the treshold.
        if (lastDistance[i] > upDistTresh || lastDistance[i] < downDistTresh)
        {
            lastDistance[i] = distance[i];
            String address = "/ext/src/" + (String)channel + "/d";
            LISAsender.send(address, (float)distance[i]);
        }
        if (lastAzimuth[i] > upAzTresh || lastAzimuth[i] < downAzTresh)
        {
            lastAzimuth[i] = azimuth[i];
            String address = "/ext/src/" + (String)channel + "/p";
            LISAsender.send("/ext/src/(k)/p", (float)azimuth[i]);
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
            REAPERsender.send("/loop/start/time", (float)0.0);
            REAPERsender.send("/loop/end/time", (float)10.0);
            REAPERsender.send("/time", (float)0.0);
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
	int scaleY = 20;
	int startBin = 300;
	int zoomLength = 228;

	for (int i = 0; i < zoomLength / 2; ++i)
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
