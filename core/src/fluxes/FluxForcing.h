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
    bool IsOk() override;

    /**
     * @copydoc Flux::GetAmount()
     */
    double GetAmount() override;

    void AttachForcing(Forcing* forcing);

    bool IsForcing() override {
        return true;
    }

  protected:
    Forcing* m_forcing;

  private:
};

#endif  // HYDROBRICKS_FLUX_FORCING_H
