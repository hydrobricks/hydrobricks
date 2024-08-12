#ifndef HYDROBRICKS_WATER_CONTAINER_H
#define HYDROBRICKS_WATER_CONTAINER_H

#include "Includes.h"
#include "Process.h"

class Brick;

class WaterContainer : public wxObject {
  public:
    WaterContainer(Brick* brick);

    virtual bool IsOk();

    void SubtractAmountFromDynamicContentChange(double change);

    void AddAmountToDynamicContentChange(double change);

    void AddAmountToStaticContentChange(double change);

    virtual void ApplyConstraints(double timeSte);

    void SetOutgoingRatesToZero();

    void Finalize();

    void Reset();

    void SaveAsInitialState();

    void SetInitialState(double content);

    vecDoublePt GetDynamicContentChanges();

    bool HasMaximumCapacity() const {
        return m_capacity != nullptr;
    }

    double GetMaximumCapacity() {
        wxASSERT(m_capacity);
        return *m_capacity;
    }

    void SetMaximumCapacity(float* value) {
        if (m_infiniteStorage) {
            throw ConceptionIssue(_("Trying to set the maximum capacity of an infinite storage."));
        }

        m_capacity = value;
    }

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

    double GetContentWithDynamicChanges() const {
        if (m_infiniteStorage) {
            return INFINITY;
        }

        return m_content + m_contentChangeDynamic;
    }

    double GetContentWithoutChanges() const {
        if (m_infiniteStorage) {
            return INFINITY;
        }

        return m_content;
    }

    double* GetContentPointer() {
        return &m_content;
    }

    void UpdateContent(double value) {
        if (m_infiniteStorage) {
            throw ConceptionIssue(_("Trying to set the content of an infinite storage."));
        }

        m_content = value;
    }

    double GetTargetFillingRatio();

    bool IsNotEmpty() {
        double content = GetContentWithChanges();
        return content > EPSILON_F && content > PRECISION;
    }

    bool HasOverflow() {
        return m_overflow != nullptr;
    }

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

    virtual bool ContentAccessible() const;

    Brick* GetParentBrick() {
        return m_parent;
    }

  protected:
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
