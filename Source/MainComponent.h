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
	OSCSender LISAsender;
    OSCSender REAPERsender;
	dsp::FFT forwardFFT;
	float fifo[fftSize];
	float fftData[2 * fftSize];
	float fftDataCopy[2 * fftSize];
	float fftDataCopyScaled[2 * fftSize];
	int fifoIndex = 0;
	bool nextFFTBlockReady = false;
    float referenceLevel = 0;
    
    // Para Elisa (and Reaper)
    float distance [5] = {0,0,0,0,0};
    float azimuth [5] = {0,0,0,0,0};
    float lastDistance [5] = {0,0,0,0,0};
    float lastAzimuth [5] = {0,0,0,0,0};
    
    float control = 0.0;
    float lastControl = 0.0;

    void sendOSCtoLISA();
    void sendOSCtoReaper(int buttonNum);

	bool reaperStopped = true;
    

	int currentSampleRate, currentBlockSize, currentFFTSize;

	AudioSetup m_audioSetup;
	TextButton openAudioDeviceManager;
	// save and load settings
	File audioSettingsFile;
	void loadSettings();
	void saveSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
