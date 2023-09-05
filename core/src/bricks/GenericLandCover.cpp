#include "GenericLandCover.h"

GenericLandCover::GenericLandCover()
    : LandCover() {}

void GenericLandCover::SetParameters(const BrickSettings& brickSettings) {
    Brick::SetParameters(brickSettings);
}

void GenericLandCover::ApplyConstraints(double timeStep) {
    Brick::ApplyConstraints(timeStep);
}

void GenericLandCover::Finalize() {
    Brick::Finalize();
}
