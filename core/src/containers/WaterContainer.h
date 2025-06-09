#ifndef HYDROBRICKS_WATER_CONTAINER_H
#define HYDROBRICKS_WATER_CONTAINER_H

#include "Includes.h"
#include "Process.h"

class Brick;

class WaterContainer : public wxObject {
  public:
    WaterContainer(Brick* brick);

    /**
     * Check if the water container is ok.
     */
    virtual bool IsOk();

    /**
     * Subtract the amount of water from the dynamic content change.
     *
     * @param change amount to subtract [mm]
     */
    void SubtractAmountFromDynamicContentChange(double change);

    /**
     * Add the amount of water to the dynamic content change.
     *
     * @param change amount to add [mm]
     */
    void AddAmountToDynamicContentChange(double change);

    /**
     * Add the amount of water to the static content change.
     *
     * @param change amount to add [mm]
     */
    void AddAmountToStaticContentChange(double change);

    /**
     * Apply the constraints to the water container.
     *
     * @param timeSte time step [s]
     */
    virtual void ApplyConstraints(double timeSte);

    /**
     * Set the outgoing rates to zero.
     */
    void SetOutgoingRatesToZero();

    /**
     * Finalize the water container computation.
     */
    void Finalize();

    /**
     * Reset the water container to its initial state.
     */
    void Reset();

    /**
     * Save the initial state of the water container.
     */
    void SaveAsInitialState();

    /**
     * Get the dynamic content changes.
     *
     * @return dynamic content changes [mm]
     */
    vecDoublePt GetDynamicContentChanges();

    /**
     * Check if the water container has a maximum capacity.
     *
     * @return true if the water container has a maximum capacity, false otherwise
     */
    bool HasMaximumCapacity() const {
        return m_capacity != nullptr;
    }

    /**
     * Get the maximum capacity of the water container.
     *
     * @return maximum capacity [mm]
     */
    double GetMaximumCapacity() {
        wxASSERT(m_capacity);
        return *m_capacity;
    }

    /**
     * Set the maximum capacity of the water container.
     *
     * @param value maximum capacity [mm]
     */
    void SetMaximumCapacity(float* value) {
        if (m_infiniteStorage) {
            throw ConceptionIssue(_("Trying to set the maximum capacity of an infinite storage."));
        }
        m_capacity = value;
    }

    /**
     * Set the water container as an infinite storage.
     */
    void SetAsInfiniteStorage() {
        m_infiniteStorage = true;
    }

    /**
     * Get the water content of the current object.
     *
     * @return water content [mm]
     */
    double GetContentWithChanges() const {
        if (m_infiniteStorage) {
            return INFINITY;
        }

        return m_content + m_contentChangeDynamic + m_contentChangeStatic;
    }

    /**
     * Get the water content of the current object with dynamic changes.
     *
     * @return water content [mm]
     */
    double GetContentWithDynamicChanges() const {
        if (m_infiniteStorage) {
            return INFINITY;
        }

        return m_content + m_contentChangeDynamic;
    }

    /**
     * Get the water content of the current object without changes.
     *
     * @return water content [mm]
     */
    double GetContentWithoutChanges() const {
        if (m_infiniteStorage) {
            return INFINITY;
        }

        return m_content;
    }

    /**
     * Get the water content value pointer.
     *
     * @return pointer to the water content value
     */
    double* GetContentPointer() {
        return &m_content;
    }

    /**
     * Set the initial state of the water container.
     *
     * @param value initial state [mm]
     */
    void SetInitialState(double value) {
        m_initialState = value;
    }

    /**
     * Update the content of the water container.
     *
     * @param value new content [mm]
     */
    void UpdateContent(double value) {
        if (m_infiniteStorage) {
            throw ConceptionIssue(_("Trying to set the content of an infinite storage."));
        }

        m_content = value;
    }

    /**
     * Get the filling ratio of the water container.
     *
     * @return filling ratio [0-1]
     */
    double GetTargetFillingRatio();

    /**
     * Check if the water container is not empty.
     *
     * @return true if the water container is not empty, false otherwise
     */
    bool IsNotEmpty() {
        double content = GetContentWithChanges();
        return content > EPSILON_F && content > PRECISION;
    }

    /**
     * Check if the water container has an overflow process.
     *
     * @return true if the water container has an overflow process, false otherwise
     */
    bool HasOverflow() {
        return m_overflow != nullptr;
    }

    /**
     * Link the water container to an overflow process.
     *
     * @param overflow pointer to the overflow process
     */
    void LinkOverflow(Process* overflow) {
        m_overflow = overflow;
    }

    /**
     * Attach incoming flux.
     *
     * @param flux incoming flux
     */
    void AttachFluxIn(Flux* flux) {
        wxASSERT(flux);
        m_inputs.push_back(flux);
    }

    /**
     * Sums the water amount from the different fluxes.
     *
     * @return sum of the water amount [mm]
     */
    virtual double SumIncomingFluxes();

    /**
     * Check if the water content is accessible.
     */
    virtual bool ContentAccessible() const;

    /**
     * Get the parent brick of the water container.
     *
     * @return pointer to the parent brick
     */
    Brick* GetParentBrick() {
        return m_parent;
    }

  private:
    double m_content;               // [mm]
    double m_contentChangeDynamic;  // [mm]
    double m_contentChangeStatic;   // [mm]
    double m_initialState;          // [mm]
    float* m_capacity;
    bool m_infiniteStorage;
    Brick* m_parent;
    Process* m_overflow;
    vector<Flux*> m_inputs;
};

#endif  // HYDROBRICKS_WATER_CONTAINER_H
