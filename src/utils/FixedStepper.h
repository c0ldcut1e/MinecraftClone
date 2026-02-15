#pragma once

class FixedStepper {
public:
    FixedStepper(double stepSeconds, double maxFrameSeconds);

    void addFrame(double deltaSeconds);

    bool shouldStep() const;
    void consumeStep();

    double getAlpha() const;
    double getStepSeconds() const;

private:
    double m_step;
    double m_maxFrame;
    double m_accumulator;
};
