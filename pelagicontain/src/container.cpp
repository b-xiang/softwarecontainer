/*
 *   Copyright (C) 2014 Pelagicore AB
 *   All rights reserved.
 */
#include <fstream>
#include <unistd.h>
#include "container.h"
#include "debug.h"
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>


#ifndef LXCTEMPLATE
	#error Must define LXCTEMPLATE as path to lxc-pelagicontain
#endif

/*
 * This directory need to exist and be set up in a special way.
 *
 * Basically something like:
 * mkdir -p /var/am/late_mounts
 * mount --bind /var/am/late_mounts /var/am/late_mounts
 * mount --make-unbindable /var/am/late_mounts
 * mount --make-shared /var/am/late_mounts
 *
 */
static std::string mountDir = "/var/am/late_mounts";

Container::Container(const std::string &name, const std::string &configFile) :
    m_name(name), m_configFile(configFile)
{
	// Make sure base_dir exist
	if (!isDirectory(mountDir.c_str()))
	{
		log_error("Directory %s does not exist.", mountDir.c_str());
		exit(-1);
	}

	// Create directories needed for mounting, will be removed in dtor
	std::string runDir = mountDir + "/" + m_name;
	if (!createDirectory(runDir.c_str()))
	{
		log_error("Could not create directory %s", runDir.c_str());
		exit(-1);
	}

	if (!createDirectory((runDir + "/bin").c_str()))
	{
		log_error("Could not create directory %s", (runDir + "/bin").c_str());
		exit(-1);
	}

	if (!createDirectory((runDir + "/shared").c_str()))
	{
		log_error("Could not create directory %s", (runDir + "/shared").c_str());
		exit(-1);
	}
}

bool Container::createDirectory(const std::string &path)
{
	if (mkdir(path.c_str(), S_IRWXU) == -1)
	{
		return false;
	}
	m_dirs.insert(m_dirs.begin(), path);
	return true;
}

bool Container::isDirectory(const std::string &path)
{
	bool isDir = false;
	struct stat st;
	if (stat(path.c_str(), &st) == 0)
	{
		if ((st.st_mode & S_IFDIR) != 0)
		{
			isDir = true;
		}
	}
	return isDir;
}

Container::~Container()
{
	// Unmount all mounted dirs
	for (std::vector<std::string>::const_iterator it = m_mounts.begin();
		 it != m_mounts.end();
		 ++it)
	{
		if (umount((*it).c_str()) == -1)
		{
			log_error("Could not unmount %s, %s", (*it).c_str(), strerror(errno));
		}
	}

	// Clean up all created directories
	for (std::vector<std::string>::const_iterator it = m_dirs.begin();
		 it != m_dirs.end();
		 ++it)
	{
		if (rmdir((*it).c_str()) == -1)
		{
			log_error("Could not remove dir %s, %s", (*it).c_str(), strerror(errno));
		}
	}
}

const char *Container::name()
{
	return m_name.c_str();
}

std::vector<std::string> Container::commands(const std::string &containedCommand,
	const std::vector<Gateway *> &gateways,
	const std::string &appRoot)
{
	int max_cmd_len = sysconf(_SC_ARG_MAX);
	char lxc_command[max_cmd_len];
	std::string environment;
	std::vector<std::string> commands;

	// Set up an environment
	for (std::vector<Gateway *>::const_iterator it = gateways.begin();
		it != gateways.end(); ++it) {
		std::string env = (*it)->environment();
		if (!env.empty())
			environment += env + " ";
	}
	log_debug("Using environment: %s", environment.c_str());

	// Command to create container
	sprintf(lxc_command,
		"MOUNT_DIR=%s DEPLOY_DIR=%s lxc-create -n %s -t %s"
		" -f %s > /tmp/lxc_%s.log",
		mountDir.c_str(),
		appRoot.c_str(),
		name(),
		LXCTEMPLATE,
		m_configFile.c_str(),
		name());
	commands.push_back(std::string(lxc_command));

	// Create command to execute inside container
	snprintf(lxc_command, max_cmd_len, "lxc-execute -n %s -- env %s %s",
		name(), environment.c_str(), containedCommand.c_str());
	commands.push_back(std::string(lxc_command));

	// Command to destroy container
	snprintf(lxc_command, max_cmd_len, "lxc-destroy -n %s", name());
	commands.push_back(std::string(lxc_command));

	return commands;
}

bool Container::bindMountDir(const std::string &src, const std::string &dst)
{
	int mount_res = mount(src.c_str(), // source
						  dst.c_str(), // target
						  "",			  // fstype
						  MS_BIND,		  // flags
						  NULL);		  // data

	if (mount_res == 0)
	{
		// Success
		m_mounts.push_back(dst);
	} else {
		// Failure
		log_error("Could not mount dir into container: src=%s, dst=%s err=%s",
				  src.c_str(),
				  dst.c_str(),
				  strerror(errno));
	}

	return (mount_res == 0);
}

/*
 * When we know which app that will be run we need to
 * do some setup, like mount in the application bin and
 * shared directories.
 */
void Container::setApplication(const std::string &appId)
{
	// The directory(ies) to be mounted is known by convention, e.g.
	// /var/am/<appId>/bin/ and /var/am/<appId>/shared/


	// bind mount /var/am/<appId>/bin/ into /var/am/late_mounts/<contid>/bin
	// this directory will be accessible in container as according to
	// the lxc-pelagicontain template
	std::string appDirBase = "/var/am/" + appId;
	std::string dstDirBase = "/var/am/late_mounts/" + m_name;
	bindMountDir(appDirBase + "/bin", dstDirBase + "/bin");
	bindMountDir(appDirBase + "/shared", dstDirBase + "/shared");
}
