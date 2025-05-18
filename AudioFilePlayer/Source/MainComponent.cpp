#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
    : state (Stopped)
{
    addAndMakeVisible(&openButton);
    openButton.setButtonText("Open...");
    openButton.onClick = [this] { openButtonClicked(); };

    addAndMakeVisible(&playButton);
    playButton.setButtonText("Play");
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.setEnabled(false);

    addAndMakeVisible(&stopButton);
    stopButton.setButtonText("Stop");
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    stopButton.setEnabled(false);

    addAndMakeVisible(&loopingToggle);
    loopingToggle.setButtonText("Loop");
    loopingToggle.onClick = [this] { loopButtonChanged(); };

    addAndMakeVisible(&currentPositionLabel);
    currentPositionLabel.setText("Stopped", juce::dontSendNotification);

    setSize(300, 200);

    formatManager.registerBasicFormats();
    transportSource.addChangeListener(this);

    setAudioChannels(0, 2);
    startTimer(20);
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()

    transportSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    // Your audio-processing code goes here!

    // For more details, see the help for AudioProcessor::getNextAudioBlock()

    if (readerSource.get() == nullptr)
    {
        bufferToFill.clearActiveBufferRegion();
        return;
    }

    transportSource.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()

    transportSource.releaseResources();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.

    openButton.setBounds(10, 10, getWidth() - 20, 20);
    playButton.setBounds(10, 40, getWidth() - 20, 20);
    stopButton.setBounds(10, 70, getWidth() - 20, 20);
    loopingToggle.setBounds(10, 100, getWidth() - 20, 20);
    currentPositionLabel.setBounds(10, 130, getWidth() - 20, 20);
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &transportSource)
    {
        if (transportSource.isPlaying()) {
            changeState(Playing);
        }
        else if ((state == Stopping) || (state == Playing)) {
            changeState(Stopped);
        }
        else if (state == Pausing) {
            changeState(Paused);
        }
    }
}

void MainComponent::timerCallback()
{
    if (transportSource.isPlaying())
    {
        juce::RelativeTime position(transportSource.getCurrentPosition());

        auto minutes = ((int)position.inMinutes()) % 60;
        auto seconds = ((int)position.inSeconds()) % 60;
        auto millis = ((int)position.inMilliseconds()) % 1000;

        auto positionString = juce::String::formatted("%02d:%02d:%03d", minutes, seconds, millis);

        currentPositionLabel.setText(positionString, juce::dontSendNotification);
    }
    else
    {
        currentPositionLabel.setText("Stopped", juce::dontSendNotification);
    }
}

void MainComponent::updateLoopState(bool shouldLoop)
{
    if (readerSource.get() != nullptr) {
        readerSource->setLooping(shouldLoop);
    }
}

void MainComponent::changeState(TransportState newState)
{
    if (state != newState)
    {
        state = newState;

        switch (state)
        {
        case Stopped:
            playButton.setButtonText("Play");
            stopButton.setButtonText("Stop");
            stopButton.setEnabled(false);
            transportSource.setPosition(0.0);
            break;

        case Starting:
            transportSource.start();
            break;

        case Playing:
            playButton.setButtonText("Pause");
            stopButton.setButtonText("Stop");
            stopButton.setEnabled(true);
            break;

        case Pausing:
            transportSource.stop();
            break;

        case Paused:
            playButton.setButtonText("Resume");
            stopButton.setButtonText("Restart");
            break;

        case Stopping:
            transportSource.stop();
            break;
        }
    }
}

void MainComponent::openButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser>("Select a file to play...",
        juce::File{},
        "*.wav;*.aif;*.aiff");
    auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync(chooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();

            if (file != juce::File{})
            {
                auto* reader = formatManager.createReaderFor(file);

                if (reader != nullptr)
                {
                    auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
                    transportSource.setSource(newSource.get(), 0, nullptr, reader->sampleRate);
                    playButton.setEnabled(true);
                    readerSource.reset(newSource.release());

                    changeState(Stopped);
                }
            }
        });
}

void MainComponent::playButtonClicked()
{
    if ((state == Stopped) || (state == Paused)) {
        changeState(Starting);
    }
    else if (state == Playing) {
        changeState(Pausing);
    }
}

void MainComponent::stopButtonClicked()
{
    if (state == Paused) {
        changeState(Stopped);
    }
    else {
        changeState(Stopping);
    }
}

void MainComponent::loopButtonChanged()
{
    updateLoopState(loopingToggle.getToggleState());
}
