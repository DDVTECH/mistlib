/// \file config.cpp
/// Contains generic functions for managing configuration.

#include "config.h"
#include "defines.h"
#include "timing.h"
#include "tinythread.h"
#include "stream.h"
#include <string.h>
#include <signal.h>

#ifdef __CYGWIN__
#include <windows.h>
#endif

#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__MACH__)
#include <sys/wait.h>
#else
#include <wait.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <errno.h>
#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <getopt.h>
#include <stdlib.h>
#include <fstream>
#include <dirent.h> //for getMyExec

bool Util::Config::is_active = false;
unsigned int Util::Config::printDebugLevel = DEBUG;//
std::string Util::Config::libver = PACKAGE_VERSION;

Util::Config::Config() {
  //global options here
  vals["debug"]["long"] = "Debug";
  vals["debug"]["short"] = "g";
  vals["debug"]["arg"] = "integer";
  vals["debug"]["help"] = "The debug level at which messages need to be printed.";
  vals["debug"]["value"].append((long long)DEBUG);
  /*capabilities["optional"]["debug level"]["name"] = "debug";
  capabilities["optional"]["debug level"]["help"] = "The debug level at which messages need to be printed.";
  capabilities["optional"]["debug level"]["option"] = "--debug";
  capabilities["optional"]["debug level"]["type"] = "integer";*/
}

/// Creates a new configuration manager.
Util::Config::Config(std::string cmd, std::string version) {
  vals.null();
  long_count = 2;
  vals["cmd"]["value"].append(cmd);
  vals["version"]["long"] = "version";
  vals["version"]["short"] = "v";
  vals["version"]["help"] = "Display library and application version, then exit.";
  vals["help"]["long"] = "help";
  vals["help"]["short"] = "h";
  vals["help"]["help"] = "Display usage and version information, then exit.";
  vals["version"]["value"].append((std::string)PACKAGE_VERSION);
  vals["version"]["value"].append(version);
  vals["debug"]["long"] = "debug";
  vals["debug"]["short"] = "g";
  vals["debug"]["arg"] = "integer";
  vals["debug"]["help"] = "The debug level at which messages need to be printed.";
  vals["debug"]["value"].append((long long)DEBUG);
}

/// Adds an option to the configuration parser.
/// The option needs an unique name (doubles will overwrite the previous) and can contain the following in the option itself:
///\code
/// {
///   "short":"o",          //The short option letter
///   "long":"onName",      //The long option
///   "short_off":"n",      //The short option-off letter
///   "long_off":"offName", //The long option-off
///   "arg":"integer",      //The type of argument, if required.
///   "value":[],           //The default value(s) for this option if it is not given on the commandline.
///   "arg_num":1,          //The count this value has on the commandline, after all the options have been processed.
///   "help":"Blahblahblah" //The helptext for this option.
/// }
///\endcode
void Util::Config::addOption(std::string optname, JSON::Value option) {
  vals[optname] = option;
  if (!vals[optname].isMember("value") && vals[optname].isMember("default")) {
    vals[optname]["value"].append(vals[optname]["default"]);
    vals[optname].removeMember("default");
  }
  long_count = 0;
  vals.forEachMember([&] (const std::string & name, const JSON::Value & val) -> bool {
    if (val.isMember("long")) {
      long_count++;
    }
    if (val.isMember("long_off")) {
      long_count++;
    }
    return true;
  });
}

/// Prints a usage message to the given output.
void Util::Config::printHelp(std::ostream & output) {
  unsigned int longest = 0;
  std::map<long long int, std::string> args;
  vals.forEachMember([&] (const std::string & name, const JSON::Value & val) -> bool {
    unsigned int current = 0;
    if (val.isMember("long")) {
      current += val["long"].asString().size() + 4;
    }
    if (val.isMember("short")) {
      current += val["short"].asString().size() + 3;
    }
    if (current > longest) {
      longest = current;
    }
    current = 0;
    if (val.isMember("long_off")) {
      current += val["long_off"].asString().size() + 4;
    }
    if (val.isMember("short_off")) {
      current += val["short_off"].asString().size() + 3;
    }
    if (current > longest) {
      longest = current;
    }
    if (val.isMember("arg_num")) {
      current = name.size() + 3;
      if (current > longest) {
        longest = current;
      }
      args[val["arg_num"].asInt()] = name;
    }
    return true;
  });
  output << "Usage: " << getString("cmd") << " [options]";
  for (std::map<long long int, std::string>::iterator i = args.begin(); i != args.end(); i++) {
    if (vals[i->second].isMember("value") && vals[i->second]["value"].size()) {
      output << " [" << i->second << "]";
    } else {
      output << " " << i->second;
    }
  }
  output << std::endl << std::endl;
  vals.forEachMember([&] (const std::string & name, const JSON::Value & val) -> bool {
    std::string f;
    if (val.isMember("long") || val.isMember("short")) {
      if (val.isMember("long") && val.isMember("short")) {
        f = "--" + val["long"].asString() + ", -" + val["short"].asString();
      } else {
        if (val.isMember("long")) {
          f = "--" + val["long"].asString();
        }
        if (val.isMember("short")) {
          f = "-" + val["short"].asString();
        }
      }
      while (f.size() < longest) {
        f.append(" ");
      }
      if (val.isMember("arg")) {
        output << f << "(" << val["arg"].asString() << ") " << val["help"].asString() << std::endl;
      } else {
        output << f << val["help"].asString() << std::endl;
      }
    }
    if (val.isMember("long_off") || val.isMember("short_off")) {
      if (val.isMember("long_off") && val.isMember("short_off")) {
        f = "--" + val["long_off"].asString() + ", -" + val["short_off"].asString();
      } else {
        if (val.isMember("long_off")) {
          f = "--" + val["long_off"].asString();
        }
        if (val.isMember("short_off")) {
          f = "-" + val["short_off"].asString();
        }
      }
      while (f.size() < longest) {
        f.append(" ");
      }
      if (val.isMember("arg")) {
        output << f << "(" << val["arg"].asString() << ") " << val["help"].asString() << std::endl;
      } else {
        output << f << val["help"].asString() << std::endl;
      }
    }
    if (val.isMember("arg_num")) {
      f = name;
      while (f.size() < longest) {
        f.append(" ");
      }
      output << f << "(" << val["arg"].asString() << ") " << val["help"].asString() << std::endl;
    }
    return true;
  });
}

/// Parses commandline arguments.
/// Calls exit if an unknown option is encountered, printing a help message.
bool Util::Config::parseArgs(int & argc, char ** & argv) {
  int opt = 0;
  std::string shortopts;
  struct option * longOpts = (struct option *)calloc(long_count + 1, sizeof(struct option));
  int long_i = 0;
  int arg_count = 0;
  vals.forEachMember([&] (const std::string & name, const JSON::Value & val) -> bool {
    if (val.isMember("short")) {
      shortopts += val["short"].asStringRef();
      if (val.isMember("arg")) {
        shortopts += ":";
      }
    }
    if (val.isMember("short_off")) {
      shortopts += val["short_off"].asStringRef();
      if (val.isMember("arg")) {
        shortopts += ":";
      }
    }
    if (val.isMember("long")) {
      longOpts[long_i].name = val["long"].asStringRef().c_str();
      longOpts[long_i].val = val["short"].asStringRef()[0];
      if (val.isMember("arg")) {
        longOpts[long_i].has_arg = 1;
      }
      long_i++;
    }
    if (val.isMember("long_off")) {
      longOpts[long_i].name = val["long_off"].asStringRef().c_str();
      longOpts[long_i].val = val["short_off"].asStringRef()[0];
      if (val.isMember("arg")) {
        longOpts[long_i].has_arg = 1;
      }
      long_i++;
    }
    if (val.isMember("arg_num") && !(val.isMember("value") && val["value"].size())) {
      if (val["arg_num"].asInt() > arg_count) {
        arg_count = val["arg_num"].asInt();
      }
    }
    return true;
  });
  while ((opt = getopt_long(argc, argv, shortopts.c_str(), longOpts, 0)) != -1) {
    switch (opt) {
      case 'h':
      case '?':
        printHelp(std::cout);
      case 'v':
        std::cout << "Library version: " PACKAGE_VERSION << std::endl;
        std::cout << "Application version: " << getString("version") << std::endl;
        exit(1);
        break;
      default:
        vals.forEachMember([&] (const std::string & name, JSON::Value & val) -> bool {
          if (val.isMember("short") && val["short"].asString()[0] == opt) {
            if (val.isMember("arg")) {
              val["value"].append((std::string)optarg);
            } else {
              val["value"].append((long long int)1);
            }
            return false;
          }
          if (val.isMember("short_off") && val["short_off"].asString()[0] == opt) {
            val["value"].append((long long int)0);
            return false;
          }
          return true;
        });
        break;
    }
  } //commandline options parser
  free(longOpts); //free the long options array
  long_i = 1; //re-use long_i as an argument counter
  while (optind < argc) { //parse all remaining options, ignoring anything unexpected.
    vals.forEachMember([&] (const std::string & name, JSON::Value & val) -> bool {
      if (val.isMember("arg_num") && val["arg_num"].asInt() == long_i) {
        val["value"].append((std::string)argv[optind]);
        return false;
      }
      return true;
    });
    optind++;
    long_i++;
  }
  if (long_i <= arg_count) {
    return false;
  }
  printDebugLevel = getInteger("debug");
  return true;
}

/// Returns a reference to the current value of an option or default if none was set.
/// If the option does not exist, this exits the application with a return code of 37.
JSON::Value & Util::Config::getOption(std::string optname, bool asArray) {
  if (!vals.isMember(optname)) {
    std::cout << "Fatal error: a non-existent option '" << optname << "' was accessed." << std::endl;
    exit(37);
  }
  if (!vals[optname].isMember("value") || !vals[optname]["value"].isArray()) {
    vals[optname]["value"].append(JSON::Value());
  }
  if (asArray) {
    return vals[optname]["value"];
  } else {
    int n = vals[optname]["value"].size();
    return vals[optname]["value"][n - 1];
  }
}

/// Returns the current value of an option or default if none was set as a string.
/// Calls getOption internally.
std::string Util::Config::getString(std::string optname) {
  return getOption(optname).asString();
}

/// Returns the current value of an option or default if none was set as a long long int.
/// Calls getOption internally.
long long int Util::Config::getInteger(std::string optname) {
  return getOption(optname).asInt();
}

/// Returns the current value of an option or default if none was set as a bool.
/// Calls getOption internally.
bool Util::Config::getBool(std::string optname) {
  return getOption(optname).asBool();
}

struct callbackData {
  Socket::Connection * sock;
  int (*cb)(Socket::Connection &);
};

static void callThreadCallback(void * cDataArg) {
  DEBUG_MSG(DLVL_INSANE, "Thread for %p started", cDataArg);
  callbackData * cData = (callbackData *)cDataArg;
  cData->cb(*(cData->sock));
  cData->sock->close();
  delete cData->sock;
  delete cData;
  DEBUG_MSG(DLVL_INSANE, "Thread for %p ended", cDataArg);
}

int Util::Config::threadServer(Socket::Server & server_socket, int (*callback)(Socket::Connection &)) {
  while (is_active && server_socket.connected()) {
    Socket::Connection S = server_socket.accept();
    if (S.connected()) { //check if the new connection is valid
      callbackData * cData = new callbackData;
      cData->sock = new Socket::Connection(S);
      cData->cb = callback;
      //spawn a new thread for this connection
      tthread::thread T(callThreadCallback, (void *)cData);
      //detach it, no need to keep track of it anymore
      T.detach();
      DEBUG_MSG(DLVL_HIGH, "Spawned new thread for socket %i", S.getSocket());
    } else {
      Util::sleep(10); //sleep 10ms
    }
  }
  server_socket.close();
  return 0;
}

int Util::Config::forkServer(Socket::Server & server_socket, int (*callback)(Socket::Connection &)) {
  while (is_active && server_socket.connected()) {
    Socket::Connection S = server_socket.accept();
    if (S.connected()) { //check if the new connection is valid
      pid_t myid = fork();
      if (myid == 0) { //if new child, start MAINHANDLER
        server_socket.drop();
        return callback(S);
      } else { //otherwise, do nothing or output debugging text
        DEBUG_MSG(DLVL_HIGH, "Forked new process %i for socket %i", (int)myid, S.getSocket());
        S.drop();
      }
    } else {
      Util::sleep(10); //sleep 10ms
    }
  }
  server_socket.close();
  return 0;
}

int Util::Config::serveThreadedSocket(int (*callback)(Socket::Connection &)) {
  Socket::Server server_socket;
  if (vals.isMember("socket")) {
    server_socket = Socket::Server(Util::getTmpFolder() + getString("socket"));
  }
  if (vals.isMember("listen_port") && vals.isMember("listen_interface")) {
    server_socket = Socket::Server(getInteger("listen_port"), getString("listen_interface"), false);
  }
  if (!server_socket.connected()) {
    DEBUG_MSG(DLVL_DEVEL, "Failure to open socket");
    return 1;
  }
  DEBUG_MSG(DLVL_DEVEL, "Activating threaded server: %s", getString("cmd").c_str());
  activate();
  return threadServer(server_socket, callback);
}

int Util::Config::serveForkedSocket(int (*callback)(Socket::Connection & S)) {
  Socket::Server server_socket;
  if (vals.isMember("socket")) {
    server_socket = Socket::Server(Util::getTmpFolder() + getString("socket"));
  }
  if (vals.isMember("listen_port") && vals.isMember("listen_interface")) {
    server_socket = Socket::Server(getInteger("listen_port"), getString("listen_interface"), false);
  }
  if (!server_socket.connected()) {
    DEBUG_MSG(DLVL_DEVEL, "Failure to open socket");
    return 1;
  }
  DEBUG_MSG(DLVL_DEVEL, "Activating forked server: %s", getString("cmd").c_str());
  activate();
  return forkServer(server_socket, callback);
}

/// Activated the stored config. This will:
/// - Drop permissions to the stored "username", if any.
/// - Daemonize the process if "daemonize" exists and is true.
/// - Set is_active to true.
/// - Set up a signal handler to set is_active to false for the SIGINT, SIGHUP and SIGTERM signals.
void Util::Config::activate() {
  if (vals.isMember("username")) {
    setUser(getString("username"));
    vals.removeMember("username");
  }
  if (vals.isMember("daemonize") && getBool("daemonize")) {
    if (vals.isMember("logfile") && getString("logfile") != "") {
      Daemonize(true);
    } else {
      Daemonize(false);
    }
    vals.removeMember("daemonize");
  }
  struct sigaction new_action;
  struct sigaction cur_action;
  new_action.sa_handler = signal_handler;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;
  sigaction(SIGINT, &new_action, NULL);
  sigaction(SIGHUP, &new_action, NULL);
  sigaction(SIGTERM, &new_action, NULL);
  sigaction(SIGPIPE, &new_action, NULL);
  //check if a child signal handler isn't set already, if so, set it.
  sigaction(SIGCHLD, 0, &cur_action);
  if (cur_action.sa_handler == SIG_DFL || cur_action.sa_handler == SIG_IGN) {
    sigaction(SIGCHLD, &new_action, NULL);
  }
  is_active = true;
}

/// Basic signal handler. Sets is_active to false if it receives
/// a SIGINT, SIGHUP or SIGTERM signal, reaps children for the SIGCHLD
/// signal, and ignores all other signals.
void Util::Config::signal_handler(int signum) {
  switch (signum) {
    case SIGINT: //these three signals will set is_active to false.
    case SIGHUP:
    case SIGTERM:
      is_active = false;
      break;
    case SIGCHLD: { //when a child dies, reap it.
        int status;
        pid_t ret = -1;
        while (ret != 0) {
          ret = waitpid(-1, &status, WNOHANG);
          if (ret < 0 && errno != EINTR) {
            break;
          }
        }
        break;
      }
    default: //other signals are ignored
      break;
  }
} //signal_handler

/// Adds the default connector options. Also updates the capabilities structure with the default options.
/// Besides the options addBasicConnectorOptions adds, this function also adds port and interface options.
void Util::Config::addConnectorOptions(int port, JSON::Value & capabilities) {
  JSON::Value option;
  option.null();
  option["long"] = "port";
  option["short"] = "p";
  option["arg"] = "integer";
  option["help"] = "TCP port to listen on";
  option["value"].append((long long)port);
  addOption("listen_port", option);
  capabilities["optional"]["port"]["name"] = "TCP port";
  capabilities["optional"]["port"]["help"] = "TCP port to listen on - default if unprovided is " + option["value"][0u].asString();
  capabilities["optional"]["port"]["type"] = "uint";
  capabilities["optional"]["port"]["option"] = "--port";
  capabilities["optional"]["port"]["default"] = option["value"][0u];

  option.null();
  option["long"] = "interface";
  option["short"] = "i";
  option["arg"] = "string";
  option["help"] = "Interface address to listen on, or 0.0.0.0 for all available interfaces.";
  option["value"].append("0.0.0.0");
  addOption("listen_interface", option);
  capabilities["optional"]["interface"]["name"] = "Interface";
  capabilities["optional"]["interface"]["help"] = "Address of the interface to listen on - default if unprovided is all interfaces";
  capabilities["optional"]["interface"]["option"] = "--interface";
  capabilities["optional"]["interface"]["type"] = "str";

  addBasicConnectorOptions(capabilities);
} //addConnectorOptions

/// Adds the default connector options. Also updates the capabilities structure with the default options.
void Util::Config::addBasicConnectorOptions(JSON::Value & capabilities) {
  JSON::Value option;
  option.null();
  option["long"] = "username";
  option["short"] = "u";
  option["arg"] = "string";
  option["help"] = "Username to drop privileges to, or root to not drop privileges.";
  option["value"].append("root");
  addOption("username", option);
  capabilities["optional"]["username"]["name"] = "Username";
  capabilities["optional"]["username"]["help"] = "Username to drop privileges to - default if unprovided means do not drop privileges";
  capabilities["optional"]["username"]["option"] = "--username";
  capabilities["optional"]["username"]["type"] = "str";


  if (capabilities.isMember("socket")) {
    option.null();
    option["arg"] = "string";
    option["help"] = "Socket name that can be connected to for this connector.";
    option["value"].append(capabilities["socket"]);
    addOption("socket", option);
  }

  option.null();
  option["long"] = "daemon";
  option["short"] = "d";
  option["long_off"] = "nodaemon";
  option["short_off"] = "n";
  option["help"] = "Whether or not to daemonize the process after starting.";
  option["value"].append(0ll);
  addOption("daemonize", option);

  option.null();
  option["long"] = "json";
  option["short"] = "j";
  option["help"] = "Output connector info in JSON format, then exit.";
  option["value"].append(0ll);
  addOption("json", option);
}



/// Gets directory the current executable is stored in.
std::string Util::getMyPath() {
  char mypath[500];
#ifdef __CYGWIN__
  GetModuleFileName(0, mypath, 500);
#else
#ifdef __APPLE__
  memset(mypath, 0, 500);
  unsigned int refSize = 500;
  int ret = _NSGetExecutablePath(mypath, &refSize);
#else
  int ret = readlink("/proc/self/exe", mypath, 500);
  if (ret != -1) {
    mypath[ret] = 0;
  } else {
    mypath[0] = 0;
  }
#endif
#endif
  std::string tPath = mypath;
  size_t slash = tPath.rfind('/');
  if (slash == std::string::npos) {
    slash = tPath.rfind('\\');
    if (slash == std::string::npos) {
      return "";
    }
  }
  tPath.resize(slash + 1);
  return tPath;
}

/// Gets all executables in getMyPath that start with "Mist".
void Util::getMyExec(std::deque<std::string> & execs) {
  std::string path = Util::getMyPath();
#ifdef __CYGWIN__
  path += "\\Mist*";
  WIN32_FIND_DATA FindFileData;
  HANDLE hdl = FindFirstFile(path.c_str(), &FindFileData);
  while (hdl != INVALID_HANDLE_VALUE) {
    execs.push_back(FindFileData.cFileName);
    if (!FindNextFile(hdl, &FindFileData)) {
      FindClose(hdl);
      hdl = INVALID_HANDLE_VALUE;
    }
  }
#else
  DIR * d = opendir(path.c_str());
  if (!d) {
    return;
  }
  struct dirent * dp;
  do {
    errno = 0;
    if ((dp = readdir(d))) {
      if (strncmp(dp->d_name, "Mist", 4) == 0) {
        execs.push_back(dp->d_name);
      }
    }
  } while (dp != NULL);
  closedir(d);
#endif
}

/// Sets the current process' running user
void Util::setUser(std::string username) {
  if (username != "root") {
    struct passwd * user_info = getpwnam(username.c_str());
    if (!user_info) {
      DEBUG_MSG(DLVL_ERROR, "Error: could not setuid %s: could not get PID", username.c_str());
      return;
    } else {
      if (setuid(user_info->pw_uid) != 0) {
        DEBUG_MSG(DLVL_ERROR, "Error: could not setuid %s: not allowed", username.c_str());
      } else {
        DEBUG_MSG(DLVL_DEVEL, "Change user to %s", username.c_str());
      }
    }
  }
}

/// Will turn the current process into a daemon.
/// Works by calling daemon(1,0):
/// Does not change directory to root.
/// Does redirect output to /dev/null
void Util::Daemonize(bool notClose) {
  DEBUG_MSG(DLVL_DEVEL, "Going into background mode...");
  int noClose = 0;
  if (notClose) {
    noClose = 1;
  }
  if (daemon(1, noClose) < 0) {
    DEBUG_MSG(DLVL_ERROR, "Failed to daemonize: %s", strerror(errno));
  }
}
