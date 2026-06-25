/* Site-specific macros for AUIS revival on macOS/Darwin */

/* Workaround: imake strips the TOP = TOPDIR line from allsys.mcr */
        TOP = TOPDIR
        IRULESRC = $(TOP)/config
