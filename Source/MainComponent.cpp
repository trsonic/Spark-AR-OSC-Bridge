#include "MainComponent.h"

MainComponent::MainComponent() : forwardFFT(fftOrder)
{
    setSize (800, 600);
	setAudioChannels(2, 0);
	startTimerHz(75);
	sender.connect("127.0.0.1", 9000);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
}

void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
	DBG("SAMPLE RATE: " + String(sampleRate) + ", FFT Size: " + String(fftSize));
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

void MainComponent::timerCallback()
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
		//						(float)fftData[56], (float)fftData[64], (float)fftData[72],
		//						(float)fftData[80], (float)fftData[88], (float)fftData[96],
		//						(float)fftData[104], (float)fftData[112], (float)fftData[120]);

		repaint();
		nextFFTBlockReady = false;
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
}

void MainComponent::resized()
{

}