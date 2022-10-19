
#ifndef PRAAS_CONTROLL_PLANE_STORAGE_HPP
#define PRAAS_CONTROLL_PLANE_STORAGE_HPP

#include <string>

namespace praas::control_plane {

  template<typename StorageImpl>
  class Storage
  {


    void create_application(std::string app_name);

    void delete_application(std::string app_name);

  };

}

#endif
