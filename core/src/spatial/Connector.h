#ifndef HYDROBRICKS_CONNECTOR_H
#define HYDROBRICKS_CONNECTOR_H

#include "Includes.h"

class SubBasin;

class Connector : public wxObject {
  public:
    Connector();

    ~Connector() override = default;

    /**
     * Connect two sub-basins.
     *
     * @param in The input sub-basin.
     * @param out The output sub-basin.
     */
    void Connect(SubBasin* in, SubBasin* out);

  protected:
    SubBasin* _in;   // non-owning reference
    SubBasin* _out;  // non-owning reference
};

#endif  // HYDROBRICKS_CONNECTOR_H
