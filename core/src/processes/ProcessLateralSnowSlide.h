#ifndef HYDROBRICKS_PROCESS_SNOWSLIDE_H
#define HYDROBRICKS_PROCESS_SNOWSLIDE_H

#include "Includes.h"
#include "Process.h"

class ProcessSnowSlide : public Process {
  public:
    explicit ProcessSnowSlide(WaterContainer* container);

    ~ProcessSnowSlide() override = default;

    /**
     * @copydoc Process::IsOk()
     */
    bool IsOk() override;

  protected:
};

#endif  // HYDROBRICKS_PROCESS_SNOWSLIDE_H
