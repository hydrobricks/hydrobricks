#include "GenericLandCover.h"

GenericLandCover::GenericLandCover()
    : LandCover() {}

void GenericLandCover::AssignParameters(const BrickSettings& brickSettings) {
    Brick::AssignParameters(brickSettings);
}

void GenericLandCover::ApplyConstraints(double timeStep) {
    Brick::ApplyConstraints(timeStep);
}

void GenericLandCover::Finalize() {
    Brick::Finalize();
}
