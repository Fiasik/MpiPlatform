#pragma once

#include "Domain/Measurement/Sample.h"

class SampleGenerator {
public:
    static std::vector<Domain::Measurement::Sample> generateForwardSequence(
        const std::vector<double>& tasks,
        double noiseLevel = 0.0,
        int pointsPerStep = 1)
    {
        std::vector<Domain::Measurement::Sample> samples;

        for (double task : tasks) {
            for (int i = 0; i < pointsPerStep; ++i) {
                Domain::Measurement::Sample s;
                s.taskPercent = task;

                // Идеальное положение
                double idealPos = task;

                // Добавление шума
                if (noiseLevel > 0) {
                    double noise = ((double)rand() / RAND_MAX - 0.5) * 2 * noiseLevel;
                    idealPos += noise;
                }

                s.positionPercent = idealPos;
                samples.push_back(s);
            }
        }

        return samples;
    }

    static std::vector<Domain::Measurement::Sample> generateCycle(
        const std::vector<double>& forwardTasks, // {0, 50, 100}
        const std::vector<double>& backwardTasks, // {100, 50, 0}
        int cycles = 2,
        double noiseLevel = 0.0)
    {
        std::vector<Domain::Measurement::Sample> samples;

        for (int cycle = 0; cycle < cycles; ++cycle) {
            // Прямой ход
            for (double task : forwardTasks) {
                Domain::Measurement::Sample s;
                s.taskPercent = task;
                s.positionPercent = task + (cycle == 1 ? 0.5 : 0.0); // имитация смещения
                if (noiseLevel > 0) {
                    s.positionPercent += ((double)rand() / RAND_MAX - 0.5) * 2 * noiseLevel;
                }
                samples.push_back(s);
            }

            // Обратный ход (если нужен)
            if (!backwardTasks.empty()) {
                for (double task : backwardTasks) {
                    Domain::Measurement::Sample s;
                    s.taskPercent = task;
                    s.positionPercent = task;
                    if (noiseLevel > 0) {
                        s.positionPercent += ((double)rand() / RAND_MAX - 0.5) * 2 * noiseLevel;
                    }
                    samples.push_back(s);
                }
            }
        }

        return samples;
    }
};
