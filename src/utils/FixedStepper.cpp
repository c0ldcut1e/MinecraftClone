#include "FixedStepper.h"

FixedStepper::FixedStepper(double stepSeconds, double maxFrameSeconds) : m_step(stepSeconds), m_maxFrame(maxFrameSeconds), m_accumulator(0.0) {}

void FixedStepper::addFrame(double deltaSeconds) {
    if (deltaSeconds > m_maxFrame) deltaSeconds = m_maxFrame;
    if (deltaSeconds < 0.0) deltaSeconds = 0.0;
    m_accumulator += deltaSeconds;
}

bool FixedStepper::shouldStep() const { return m_accumulator >= m_step; }

void FixedStepper::consumeStep() { m_accumulator -= m_step; }

double FixedStepper::getPartialTicks() const {
    if (m_step <= 0.0) return 0.0;
    double a = m_accumulator / m_step;
    if (a < 0.0) a = 0.0;
    if (a > 1.0) a = 1.0;
    return a;
}

double FixedStepper::getStepSeconds() const { return m_step; }
