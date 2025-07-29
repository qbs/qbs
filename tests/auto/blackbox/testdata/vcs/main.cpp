#include <my-repo-state.h>
#include <iostream>

int main()
{
    std::cout << "__"
              << "repoState=" << VCS_REPO_STATE << ";"
              << "latestTag=" << VCS_REPO_LATEST_TAG << ";"
              << "commitsSinceTag=" << VCS_REPO_COMMITS_SINCE_TAG << ";"
              << "commitSha=" << VCS_REPO_COMMIT_SHA << "__" << std::endl;
}
