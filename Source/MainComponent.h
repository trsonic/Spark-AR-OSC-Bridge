#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "AudioSetup.h"

class MainComponent   : public AudioAppComponent,
						public Button::Listener,
						public Timer,
						public OSCSender
{
public:
    MainComponent();
    ~MainComponent();

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
	void buttonClicked(Button* buttonThatWasClicked) override;
	void timerCallback() override;
	void pushNextSampleIntoFifo(float sample);
    
    void paint (Graphics& g) override;
    void resized() override;

	enum
	{
		fftOrder = 10,
		fftSize = 1 << fftOrder
	};

private:
	OSCSender sender;
	dsp::FFT forwardFFT;
	float fifo[fftSize];
	float fftData[2 * fftSize];
	float fftDataCopy[2 * fftSize];
	float fftDataCopyScaled[2 * fftSize];
	int fifoIndex = 0;
	bool nextFFTBlockReady = false;

	int currentSampleRate, currentBlockSize, currentFFTSize;

	AudioSetup m_audioSetup;
	TextButton openAudioDeviceManager;
	// save and load settings
	File audioSettingsFile;
	void loadSettings();
	void saveSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
