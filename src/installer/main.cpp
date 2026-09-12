#include <gtk/gtk.h>
#include <iostream>
#include "installer/installer_window.hpp"

int main(int argc, char* argv[]) {
    // Initialize GTK
    gtk_init(&argc, &argv);

    g_set_prgname("lumen-installer");
    g_set_application_name("lumen setup wizard");

    // Instantiate and display main setup wizard
    Blueprint::Installer::InstallerWindow installer;

    installer.show();

    int initialStep = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--step" && i + 1 < argc) {
            initialStep = std::atoi(argv[++i]);
        }
    }
    if (initialStep > 0) {
        installer.setStep(initialStep);
    }

    // Enter main event loop
    gtk_main();

    return 0;
}
