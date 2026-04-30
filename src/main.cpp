#include "rt_ppr.hpp"

#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

int main()
{
    // Ensure /var/lib/amd-ppr/ exists
    struct stat sb{};
    if (stat(kPprDir.data(), &sb) != 0)
    {
        if (mkdir(kPprDir.data(), 0777) != 0)
        {
            sd_journal_print(LOG_ERR, "bmc-ppr: failed to create dir %s: %s\n",
                             kPprDir.data(), strerror(errno));
        }
    }

    // Runtime Soft PPR — inotify-based file watcher.
    try
    {
        RtPprManager rtPpr{RAS_WATCH_DIR, RT_PPR_CONFIG_FILE};
        rtPpr.run();
    }
    catch (const std::exception& e)
    {
        sd_journal_print(LOG_ERR, "RtPprManager fatal error: %s\n", e.what());
        return 1;
    }

    return 0;
}
