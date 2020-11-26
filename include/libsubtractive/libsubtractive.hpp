#ifndef LIBSUBTRACTIVE_LIBSUBTRACTIVE_HPP
#define LIBSUBTRACTIVE_LIBSUBTRACTIVE_HPP

extern "C" {
enum LS_Options {
    LS_LISTDEVICES = 1,
    LS_SUBSCRIBE = 2,
    LS_UNSUBSCRIBE = 3,
    LS_SENDGCODE = 4,
    LS_EXECUTE_PROGRAM = 5,
    LS_GRBLHELP = 6,
    LS_GRBLSTATUS = 7,
    LS_GRBLSETTINGS = 8,
    LS_GRBLVERSION = 9,
    LS_GRBLHOME = 10,
    LS_GRBLPARAMS = 11,
    LS_GRBLPARSERSTATE = 12,
    LS_GRBLSTARTUPBLOCKS = 13,
    LS_GRBLCHECKMODETOGGLE = 14,
    LS_GRBLRESETALARM = 15,
    LS_GRBLSOFTRESET = 16,
    LS_GRBLCYCLETOGGLE = 17,
    LS_GRBLFEEDHOLD = 18,
    LS_GRBLJOGCANCEL = 19,
    LS_RESPONSERECEIVED = 123,
    LS_NOWEXECUTING = 124,
    LS_DEVICEREMOVED = 125,
    LS_DEVICEADDED = 126,
    LS_LISTDEVICES_REPLY = 127,
};

struct LS_options {
    bool init_usb_;
};

LS_options libsubtractive_default_options();
const char* libsubtractive_endpoint();
void* libsubtractive_init_context(const LS_options* options);
void libsubtractive_close_context();
}

#endif  // LIBSUBTRACTIVE_LIBSUBTRACTIVE_HPP
