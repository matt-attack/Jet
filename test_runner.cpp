
/*#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif*/

#include <vector>
#include <string>

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <fstream>

// returns the return code
int run_command(const std::string& command, std::string* output = 0)
{
  FILE *fp;
  char path[1035];

  /* Open the command for reading. */
  fp = popen(command.c_str(), "r");
  if (fp == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  /* Read the output a line at a time - output it. */
  while (fgets(path, sizeof(path), fp) != NULL) {
    printf("    %s", path);
    if (output)
    {
      *output += path;
    }
  }

  /* close */
  return pclose(fp);
}

std::vector<std::string> find_tests(const char *name)
{
    std::vector<std::string> tests;

    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return tests;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            char path[1024];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            //printf("%*s[%s]\n", 0, "", entry->d_name);
            //listdir(path, indent + 2);

            // now check if theres a test file inside
            std::string test_path = path;
            test_path += "/test";
            FILE* f = fopen(test_path.c_str(), "rb");
            if (f)
            {
              fclose(f);
              tests.push_back(path);
              printf("Found test %s\n", path);
            }
        } else {
            //printf("%*s- %s\n", 0, "", entry->d_name);
        }
    }
    closedir(dir);
    return tests;
}

std::string read_file(const std::string& fileName)
{
    std::ifstream ifs(fileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

    std::ifstream::pos_type fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<char> bytes(fileSize);
    ifs.read(bytes.data(), fileSize);

    return std::string(bytes.data(), fileSize);
}

std::vector<std::string> split(const std::string& str, char delim, int rep = 1) {
    std::vector<std::string> flds;
    std::string work = str;
    std::string buf = "";
    int i = 0;
    while (i < work.length()) {
        if (work[i] != delim)
            buf += work[i];
        else if (rep == 1) {
            flds.push_back(buf);
            buf = "";
        } else if (buf.length() > 0) {
            flds.push_back(buf);
            buf = "";
        }
        i++;
    }
    if (!buf.empty())
        flds.push_back(buf);
    return flds;
}

std::string strip(const std::string& in)
{
  // strip out control chars
  std::string out;
  for (int i = 0; i < in.length(); i++)
  {
    if (in[i] == 0x1b)
    {
      // skip up to an m
      do
      {
        i++;
      }
      while (i < in.length() && in[i] != 'm');
      continue;
    }
    out += in[i];
  }
  return out;
}

std::string testf(std::string in)
{
  std::string out;
  for (int i = 0; i < in.length(); i++)
  {
    /*if (true)//in[i] == ' ')
    {
      out += '*';
      continue;
    }
    if (in[i] == '\r')
    {
      out += '?';
      continue;
    }*/
    out += in[i];
  }
  return out;
}



int main(int argc, char* argv[])
{
  if (argc < 3)
  {
    printf("Not enough arguments.\n");
    return -1;
  }

  // first argument needs to be the path to jet
  std::string jet = argv[1];

  // okay, second argument needs to be the location of the tests
  std::string location = argv[2];
  auto tests = find_tests(location.c_str());

  // go through and run each test one by one (can do this in parallel later)
  int success = 0;
  std::vector<std::string> failed;
  for (auto& test: tests)
  {
    printf("Building test '%s'...\n", test.c_str());

    /* Open the command for reading. */
    std::string command = "./" + jet + " build " + test + " -f";
    std::string build_output;
    int compile_code = run_command(command, &build_output);

    printf("Build complete.\n");

    // now check if we needed to compare with an error or not by reading the test file
    std::string test_desc_loc = test + "/test";
    std::string desired_output = read_file(test_desc_loc);
    bool should_fail = (desired_output.length() != 0);
    //printf("%s", desired_output.c_str());
    if (!should_fail && compile_code)
    {
      printf("Test '%s' failed.\n\n", test.c_str());
      failed.push_back(test);
      continue;
    }
    else if (should_fail)
    {
      // dont actually run the output program, there shouldnt be one
      // just compare the output with that of the file line by line
      build_output = strip(build_output);
      auto build_split = split(build_output, '\n');
      auto desired_split = split(desired_output, '\n');
      //printf("Built: %li Desired %li\n", build_split.size(), desired_split.size());
      /*for (auto& t: build_split)
      {
        printf("%s\n", testf(t).c_str());
      }

      for (auto& t: desired_split)
      {
        printf("%s\n", testf(t).c_str());
      }*/

      if (build_split.size() != desired_split.size())
      {
        printf("Output doesnt have enough lines\n");
        printf("Test '%s' failed.\n\n", test.c_str());
        failed.push_back(test);
        continue;
      }

      // now compare line by line
      bool diff = false;
      for (int i = 0; i < build_split.size(); i++)
      {
        if (build_split[i] != desired_split[i])
        {
          printf("Output line %i is different than expected\n", i);
          printf("%s\nvs\n%s\n", build_split[i].c_str(), desired_split[i].c_str());
          diff = true;
        }
        if (desired_split[i].find('\r') != -1)
        {
          printf("has \\r");
        }
      }

      if (diff)
      {
        printf("Test '%s' failed.\n\n", test.c_str());
        failed.push_back(test);
        continue;
      }
      
      printf("Build test succeeded.\n");
      success++;
      continue;
    }

    // If we didnt and weren't supposed to error, run the program to look for test failures
    printf("Running test...\n");
    // now run the test code itself (for the moment force the executable to be called test)
    std::string executable_name = "test";
    command = "./" + test + "/build/" + executable_name;
    int code = run_command(command);
    printf("Test complete. Return code %i\n\n", code);

    success++;
  }

  // print out total status
  printf("\nFinished. %i tests successful. %li tests failed.\n", success, failed.size());

  if (failed.size())
  {
    printf("\n");
    for (auto& t: failed)
    {
      printf("Test: '%s' failed\n", t.c_str());
    }
  }

  return 0;
}


