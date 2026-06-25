/* Site-specific macros for AUIS revival on macOS/Darwin */

/* Override INCLUDES to use source tree paths instead of installed paths */
        INCLUDES = -I$(TOP)/config -I$(TOP)/config/darwin -I$(TOP)/overhead/class/lib -I$(TOP)/overhead/cmenu -I$(TOP)/overhead/util/hdrs -I$(TOP)/atk/basics/common -I$(TOP)/atk/support -I$(TOP)/atk/supportviews -I$(TOP)/atk/text -I$(XINCDIR)
