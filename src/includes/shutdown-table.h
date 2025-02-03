#ifndef SHUTDOWN_TABLE_H
#define SHUTDOWN_TABLE_H

#include <mconfig.h>

#include <service-constants.h>

#include <cstdlib> // bsearch
#include <sys/reboot.h>

const struct shutdown_table_t {
    static constexpr const struct row {
        shutdown_type_t type;
        char opt; // '\0' if N/A
        int64_t rb_cmd; // -1 if N/A, -2 if unsupported
    } tbl[] = {
        { shutdown_type_t::REMAIN, '\0', -1 },
        { shutdown_type_t::HALT, 'h',
#ifdef RB_HALT_SYSTEM
          RB_HALT_SYSTEM
#elif defined RB_HALT
          RB_HALT
#else
          -2
#endif
        },
        { shutdown_type_t::POWEROFF, 'p',
#ifdef RB_POWER_OFF
          RB_POWER_OFF
#else
          -2
#endif
        },
        { shutdown_type_t::REBOOT, 'r', RB_AUTOBOOT },
        { shutdown_type_t::SOFTREBOOT, 's', -1 },
        { shutdown_type_t::KEXEC, 'k',
#ifdef RB_KEXEC
          RB_KEXEC
#else
          -2
#endif
        }
    };

private:
    static int cmp_shutdown(const void *a, const void *b) {
        return static_cast<char>(((row*)b)->type) - *(const char*)a;
    }
public:
    const struct row *find(const shutdown_type_t type) const {
        return static_cast<row*>(
            std::bsearch(&type, tbl, sizeof tbl / sizeof *tbl, sizeof *tbl, cmp_shutdown)
        );
    }
} shutdown_table;

constexpr const struct shutdown_table_t::row shutdown_table_t::tbl[];


#endif
