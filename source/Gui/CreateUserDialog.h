#pragma once

#include "Definitions.h"

class _CreateUserDialog
{
public:
    _CreateUserDialog(ActivateUserDialog const& activateUserDialog, NetworkController const& networkController);
    ~_CreateUserDialog();

    void process();

    void show(std::string const& userName, std::string const& password);

private:
    void onCreateUser();

    NetworkController _networkController;
    ActivateUserDialog _activateUserDialog; 

    bool _show = false;
    std::string _userName;
    std::string _password;
    std::string _email;
};