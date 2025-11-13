#ifndef HYDROBRICKS_FLUX_FORCING_H
#define HYDROBRICKS_FLUX_FORCING_H

#include "Flux.h"
#include "Forcing.h"
#include "Includes.h"

class FluxForcing : public Flux {
  public:
    explicit FluxForcing();

    /**
     * @copydoc Flux::IsOk()
     */
    [[nodiscard]] bool IsOk() override;

    /**
     * @copydoc Flux::GetAmount()
     */
    double GetAmount() override;

    /**
     * Attach a forcing to the flux.
     *
     * @param forcing the forcing to attach.
     */
    void AttachForcing(Forcing* forcing);

    /**
     * @copydoc Flux::IsForcing()
     */
    bool IsForcing() override {
        return true;
    }

  protected:
    Forcing* _forcing;
};

#endif  // HYDROBRICKS_FLUX_FORCING_H
