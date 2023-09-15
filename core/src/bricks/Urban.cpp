#include "Urban.h"

Urban::Urban()
    : LandCover() {}

void Urban::SetParameters(const BrickSettings& brickSettings) {
    Brick::SetParameters(brickSettings);
}

void Urban::ApplyConstraints(double timeStep) {
    Brick::ApplyConstraints(timeStep);
}

void Urban::Finalize() {
    Brick::Finalize();
}
