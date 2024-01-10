Getting Started
===============
This chapter is intended to get you up and running with MeshNOW in no time!
We show you how to install MeshNOW and how to use it to create a simple mesh network.
Knowledge of Espressif Systems `ESP-IDF <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/index.html>`_ is a prerequisite to using MeshNOW.

Supported ESP-IDF Versions
--------------------------
MeshNOW targets ESP-IDF **v5.0.1** and makes use of the available C++20 features. Newer ESP-IDF versions should be compatible but are not tested.
If you're required to use an older version of ESP-IDF, the following steps will not work for you.
However, you can still use MeshNOW by manually copying the files from the Git repository into your project.
Be sure to also include the dependencies.

.. note::
    We do not support any other method of installation besides the one listed below, so you're on your own if you choose to do it manually.
    Support with any significantly older version is also not guaranteed.


Installing MeshNOW
------------------
MeshNOW exposes itself as a single ESP-IDF component that can easily be added to your project!
The most convenient way is to use the `ESP-IDF Component Manager <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html>`_.

#. Install, enable, and configure the required tooling:

   * ESP-IDF: If you don't have the correct version installed, follow the `ESP-IDF Getting Started Guide <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html>`_.
   * ESP-IDF Component Manager: If you're using a recent version ESP-IDF, it is enabled by default. Otherwise, follow the their `getting started guide <https://docs.espressif.com/projects/idf-component-manager/en/latest/getting_started/index.html>`_.

#. Prepare a project:

   #. Create a new ESP-IDF project as usual or use an existing one.
   #. Open a terminal and navigate to your project directory.

#. Add MeshNOW as a dependency to a component of your choosing, e.g., the ``main`` component:

   #. Create the ``idf_component.yml`` manifest file in the component's directory if you haven't already. You can do so by running ``idf.py create-manifest --component=<my_component>`` where ``<my_component>`` is the name of your component, e.g., ``main``.
   #. Add a new ``dependencies`` entry named ``meshnow`` to the manifest file and set its ``git`` url field to ``https://github.com/derkalaender/MeshNOW.git``

   .. note::
       Your ``idf_component.yml`` manifest should look something like this:

       .. code-block:: yaml

           dependencies:
             idf:
               version: ">=5.0.1"
             meshnow:
               git: "https://github.com/derkalaender/MeshNOW.git"
#. Run ``idf.py reconfigure`` to reconfigure the project and let the component manager discover and download MeshNOW and its dependencies.
#. Add ``meshnow`` to your ``PRIV_REQUIRES`` (or ``REQUIRES`` if you're exposing MeshNOW in your public include files) in the component's ``CMakeLists.txt`` file.
#. MeshNOW needs a few configuration options to be enabled by the user. You can do this by invoking ``idf.py menuconfig`` and enabling the following options:

   * Component config -> LWIP -> Enable copy between Layer2 and Layer packets
   * Component config -> LWIP -> Enable IP forwarding
   * Component config -> LWIP -> Enable NAT (new/experimental)

   Alternatively, you can also add the options to your ``sdkconfig`` file in the project's root directory directly:

   .. code-block:: ini

       CONFIG_LWIP_L2_TO_L3_COPY=y
       CONFIG_LWIP_IP_FORWARD=y
       CONFIG_LWIP_NAT=y

Done! You can now use MeshNOW in your project.


What's Next?
------------
Explore the rest of the documentation!

See the :doc:`examples/index` for some inspiration on how to use MeshNOW.

You can also directly check out the :doc:`api`. It should feel familiar if you've used the ESP-IDF and its networking capabilities before.

The :doc:`sdkconfig` page lists all the configuration options that MeshNOW exposes for maximum fine-tuning.
