#include "Vegetation.h"

Vegetation::Vegetation()
    : LandCover() {
    _category = BrickCategory::Vegetation;
}

void Vegetation::SetParameters(const BrickSettings& brickSettings) {
    Brick::SetParameters(brickSettings);
}

void Vegetation::ApplyConstraints(double timeStep) {
    Brick::ApplyConstraints(timeStep);
}

void Vegetation::Finalize() {
    Brick::Finalize();
}
