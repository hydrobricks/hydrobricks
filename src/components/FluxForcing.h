#ifndef HYDROBRICKS_FLUX_FORCING_H
#define HYDROBRICKS_FLUX_FORCING_H

#include "Includes.h"
#include "Flux.h"
#include "Forcing.h"

class FluxForcing : public Flux {
  public:
    explicit FluxForcing();

    /**
     * @copydoc Flux::IsOk()
     */
    bool IsOk() override;

    /**
     * @copydoc Flux::GetOutgoingAmount()
     */
    double GetOutgoingAmount() override;

    void AttachForcing(Forcing* forcing);

  protected:
    Forcing* m_forcing;

  private:
};

#endif  // HYDROBRICKS_FLUX_FORCING_H
