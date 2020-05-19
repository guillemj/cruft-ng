#include <iostream>
#include <fstream>
#include <algorithm>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include "dpkg.h"
#include "usr_merge.h"

int read_dpkg_header(vector<string>& packages)
{
	bool debug=getenv("DEBUG") != NULL;

	// TODO: read DPKG database using C library instead of using dpkg-query
	if (debug) cerr << "DPKG DATA\n";
	FILE* fp;
	if ((fp = popen("dpkg-query --show --showformat '${binary:Package}\n'", "r")) == NULL) return 1;
	const int SIZEBUF = 4096;
	char buf[SIZEBUF];
	string package;
	while (fgets(buf, sizeof(buf),fp))
	{
		package=buf;
		package=package.substr(0,package.size() - 1); // remove '/n'
		//cerr << package << endl;
		packages.push_back(package);
	}
	pclose(fp);
	if (debug) cerr << packages.size() << " packages installed"  << endl << endl;
	return 0;
}

int read_diversions_old(vector<Diversion>& diversions)
{
	ifstream txt("/var/lib/dpkg/diversions");
	while(!txt.eof())
	{
		string oldfile,newfile,package;
		getline(txt,oldfile);
		getline(txt,newfile);
		getline(txt,package);
		diversions.push_back(Diversion(oldfile,newfile,package));
	}
	txt.close();
	return 0;
}

int read_diversions(vector<Diversion>& diversions)
{
	bool debug=getenv("DEBUG") != NULL;

	FILE* fp;
        setenv("LANG", "C", 1);
	if ((fp = popen("dpkg-divert --list", "r")) == NULL) return 1;

	const int SIZEBUF = 4096;
	char buf[SIZEBUF];
	while (fgets(buf, sizeof(buf),fp))
	{
		const char delim = ' ';
		string oldfile,newfile,package;

                //diversion of /usr/share/dict/words to /usr/share/dict/words.pre-dictionaries-common by dictionaries-common
                //diversion of /usr/share/man/man1/sh.1.gz to /usr/share/man/man1/sh.distrib.1.gz by dash
                //diversion of /usr/bin/firefox to /usr/bin/firefox.real by firefox-esr
                //diversion of /bin/sh to /bin/sh.distrib by dash

		char* token = strtok((char*)buf, &delim);

		strtok(NULL, &delim);

		token = strtok(NULL, &delim);
		oldfile = token;

		strtok(NULL, &delim);

		token = strtok(NULL, &delim);
		newfile = token;

		strtok(NULL, &delim);

		token = strtok(NULL, &delim);
		package = token;

		diversions.push_back(Diversion(oldfile,newfile,package));
	}
	pclose(fp);
	if (debug) cerr << diversions.size() << " files diverted"  << endl << endl;

	return 0;
}

int read_dpkg_items(vector<string>& dpkg)
{
	bool debug=getenv("DEBUG") != NULL;

	if (debug) cerr << "READING FILES IN DPKG DATABASE" << endl;
	vector<Diversion> diversions;
	read_diversions(diversions);

	// TODO: read DPKG database instead of using dpkg-query
	// cat /var/lib/dpkg/info/ *.list |sort -u
        string command="dpkg-query --listfiles $(dpkg-query --show --showformat '${binary:Package} ')|sort -u";
	const int SIZEBUF = 200;
	char buf[SIZEBUF];
	FILE* fp;
	if ((fp = popen(command.c_str(), "r")) == NULL) return 1;
	while (fgets(buf, sizeof(buf),fp))
	{
		string filename=buf;
		if (filename.substr(0,1)!="/") continue;
		filename=filename.substr(0,filename.size() - 1);
		// TODO: ignore ${prunepaths} here also
		vector<Diversion>::iterator it=diversions.begin();
		struct stat stat_buffer;
		for(;it !=diversions.end();it++) {
			if (filename==(*it).oldfile
		            && stat(filename.c_str(),&stat_buffer)!= 0) filename=(*it).newfile;
		}

		dpkg.push_back(usr_merge(filename));


		// also consider all intermediate subdirectories under /etc
		if (filename.substr(0,5)!="/etc/")
			continue;

		if (stat(filename.c_str(),&stat_buffer) == 0)
			if ((stat_buffer.st_mode & S_IFDIR) != 0)
				continue;

		while (1)
		{
			size_t found;
			found=filename.find_last_of("/");
			filename=filename.substr(0,found);
			if (filename == "/etc")
				break;

			dpkg.push_back(filename);
		}
	}
        pclose(fp);
	if (debug) cerr << "done"  << endl;
	sort(dpkg.begin(), dpkg.end());
	// remove duplicates
	dpkg.erase( unique( dpkg.begin(), dpkg.end() ), dpkg.end() );
	if (debug) cerr << dpkg.size() << " files in DPKG database" << endl;
	return 0;
}
