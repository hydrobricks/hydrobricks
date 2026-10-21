#ifndef HYDROBRICKS_CONNECTOR_H
#define HYDROBRICKS_CONNECTOR_H

#include "Includes.h"

class Subbasin;

class Connector {
  public:
    Connector();

    virtual ~Connector() = default;

    /**
     * Connect two subbasins.
     *
     * @param in The input subbasin.
     * @param out The output subbasin.
     */
    void Connect(Subbasin* in, Subbasin* out);

  protected:
    Subbasin* _in;   // non-owning reference
    Subbasin* _out;  // non-owning reference
};

#endif  // HYDROBRICKS_CONNECTOR_H
