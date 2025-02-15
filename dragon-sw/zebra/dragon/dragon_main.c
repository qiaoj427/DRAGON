/*
 * OSPFd main routine.
 *   Copyright (C) 1998, 99 Kunihiro Ishiguro, Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <zebra.h>

#include "version.h"
#include "getopt.h"
#include "thread.h"
#include "prefix.h"
#include "linklist.h"
#include "if.h"
#include "vector.h"
#include "vty.h"
#include "command.h"
#include "filter.h"
#include "plist.h"
#include "stream.h"
#include "log.h"
#include "memory.h"
#include "ast_master/dragon_app.h"
#include "ast_master/ast_master_ext.h"
#include "dragon/dragond.h"

struct in_addr srcAddr; 
u_int16_t srcPort;
struct in_addr destAddr;	
u_int16_t destPort;

/* Configuration filename and directory. */
char config_current[] = DRAGON_DEFAULT_CONFIG;
char config_default[] = SYSCONFDIR DRAGON_DEFAULT_CONFIG;

/* DRAGONd options. */
struct option longopts[] = 
{
  { "daemon",      no_argument,       NULL, 'd'},
  { "config_file", required_argument, NULL, 'f'},
  { "pid_file",    required_argument, NULL, 'i'},
  { "log_mode",    no_argument,       NULL, 'l'},
  { "help",        no_argument,       NULL, 'h'},
  { "vty_addr",    required_argument, NULL, 'A'},
  { "vty_port",    required_argument, NULL, 'P'},
  { "version",     no_argument,       NULL, 'v'},
  { 0 }
};

/* Global DRAGON master */
struct dragon_master dmaster;
struct thread_master *master; /* master = dmaster.master */

/* Process ID saved for use by init system */
char *pid_file = PATH_DRAGON_PID;

/* Help information display. */
static void
usage (char *progname, int status)
{
  if (status != 0)
    fprintf (stderr, "Try `%s --help' for more information.\n", progname);
  else
    {    
      printf ("Usage : %s [OPTION...]\n\
NSF DRAGON gateway daemon.\n\n\
-d, --daemon       Runs in daemon mode\n\
-f, --config_file  Set configuration file name\n\
-i, --pid_file     Set process identifier file name\n\
-A, --vty_addr     Set vty's bind address\n\
-P, --vty_port     Set vty's port number\n\
-v, --version      Print program version\n\
-h, --help         Display this help and exit\n\
\n\
Report bugs to %s\n", progname, ZEBRA_BUG_ADDRESS);
    }
  exit (status);
}
 
/* SIGHUP handler. */
void 
sighup (int sig)
{
  zlog (NULL, LOG_INFO, "SIGHUP received");
}

/* SIGINT handler. */
void
sigint (int sig)
{
  struct listnode *node;
  struct lsp *lsp = NULL;

  zlog (NULL, LOG_INFO, "Terminating on signal");
  LIST_LOOP(dmaster.dragon_lsp_table, lsp , node)
  {
  	/* TODO: inform NARB upon termination */
	switch (lsp->status)
	{
		case LSP_IS:
		case LSP_COMMIT:
		case LSP_DELETE:
		case LSP_LISTEN:
		     zTearRsvpPathRequest(dmaster.api, &lsp->common);	
			break;

		default: 
			break;
  	}
  }

  exit (0);
}

/* SIGUSR1 handler. */
void
sigusr1 (int sig)
{
  zlog_rotate (NULL);
}

/* Signal wrapper. */
RETSIGTYPE *
signal_set (int signo, void (*func)(int))
{
  int ret;
  struct sigaction sig;
  struct sigaction osig;

  sig.sa_handler = func;
  sigemptyset (&sig.sa_mask);
  sig.sa_flags = 0;
#ifdef SA_RESTART
  sig.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */

  ret = sigaction (signo, &sig, &osig);

  if (ret < 0) 
    return (SIG_ERR);
  else
    return (osig.sa_handler);
}

/* Initialization of signal handles. */
void
signal_init ()
{
  signal_set (SIGHUP, sighup);
  signal_set (SIGINT, sigint);
  signal_set (SIGTERM, sigint);
  signal_set (SIGPIPE, SIG_IGN);
#ifdef SIGTSTP
  signal_set (SIGTSTP, SIG_IGN);
#endif
#ifdef SIGTTIN
  signal_set (SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTTOU
  signal_set (SIGTTOU, SIG_IGN);
#endif
  signal_set (SIGUSR1, sigusr1);
}

static void
init_application_module()
{
  /* DEVELOPER: add your resource module in here
   * dragon_app serves as an example, please consult dragon_app.[ch]
   */
  init_dragon_module();
}
 
/* DRAGONd main routine. */
int
main (int argc, char **argv)
{
  char *p;
  char *vty_addr = NULL;
  int vty_port = 0;
  int daemon_mode = 0;
  char *config_file = NULL;
  char *progname;
  struct thread thread;
  int xml_mode = 1;

  /* Set umask before anything for security */
  umask (0027);

  /* get program name */
  progname = ((p = strrchr (argv[0], '/')) ? ++p : argv[0]);

  /* Invoked by a priviledged user? -- endo. */
  if (getuid () != 0)
    {
      errno = EPERM;
      perror (progname);
      exit (1);
    }

  zlog_default = openzlog (progname, ZLOG_NOLOG, ZLOG_DRAGON,
			   LOG_CONS|LOG_NDELAY|LOG_PID, LOG_DAEMON);

  zlog_set_file(zlog_default, ZLOG_FILE, "/var/log/dragon.log");

  while (1) 
    {
      int opt;

      opt = getopt_long (argc, argv, "dlf:hA:P:v", longopts, 0);
    
      if (opt == EOF)
	break;

      switch (opt) 
	{
	case 0:
	  break;
	case 'd':
	  daemon_mode = 1;
	  break;
	case 'f':
	  config_file = optarg;
	  break;
	case 'A':
	  vty_addr = optarg;
	  break;
        case 'i':
          pid_file = optarg;
          break;
	case 'P':
	  vty_port = atoi (optarg);
	  break;
	case 'v':
	  print_version (progname);
	  exit (0);
	  break;
	case 'h':
	  usage (progname, 0);
	  break;
	default:
	  usage (progname, 1);
	  break;
	}
    }

  /* Initialize dragon master */
  if (dragon_master_init() < 0)
  	return (-1);

  /* init all dragon_app related stuff */
  init_application_module();
  if (init_resource()) {
    zlog_warn("There is no resource defined in this ast_master instance; exit ..");
    xml_mode = 0;
  }
  if (xml_mode) {
    init_schema("/usr/local/ast_file/xml_schema/setup_req.rng");
  }

  /* Library inits. */
  signal_init ();
  dragon_cmd_init (dragon_config_write);
  dragon_vty_init ();
  dragon_supp_vty_init();
  sort_node ();

  /* Get configuration file. */
  vty_read_config (config_file, config_current, config_default);

  /* Change to the daemon program. */
  if (daemon_mode)
    daemon (0, 0);

  /* Process id file create. */
  pid_output (pid_file);

  /* Print banner. */
  zlog (NULL, LOG_INFO, "DRAGONd starts ...");
  zlog (NULL, LOG_INFO, "%s", dragon_version_string());

  /* Create VTY socket */
  vty_serv_sock (vty_addr,
		 vty_port ? vty_port : DRAGON_VTY_PORT, DRAGON_VTYSH_PATH);

  if (xml_mode) {
    /* init xml related stuff */
    /*DRAGON_XML_PORT defined in ast_master/ast_master.h*/
    xml_serv_sock (vty_addr, DRAGON_XML_PORT, DRAGON_XML_PATH); 
  }

  /* Fetch next active thread. */
  while (thread_fetch (master, &thread))
    thread_call (&thread);

  /* Not reached. */
  exit (0);
}

