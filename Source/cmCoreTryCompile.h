/* Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
   file Copyright.txt or https://cmake.org/licensing for details.  */
#pragma once

#include "cmConfigure.h" // IWYU pragma: keep

#include <string>
#include <vector>

#include "cmStateTypes.h"

class cmMakefile;

/** \class cmCoreTryCompile
 * \brief Base class for cmTryCompileCommand and cmTryRunCommand
 *
 * cmCoreTryCompile implements the functionality to build a program.
 * It is the base class for cmTryCompileCommand and cmTryRunCommand.
 */
class cmCoreTryCompile
{
public:
  cmCoreTryCompile(cmMakefile* mf)
    : Makefile(mf)
  {
  }

  /**
   * This is the core code for try compile. It is here so that other
   * commands, such as TryRun can access the same logic without
   * duplication.
   */
  int TryCompileCode(std::vector<std::string> const& argv, bool isTryRun);

  /**
   * This deletes all the files created by TryCompileCode.
   * This way we do not have to rely on the timing and
   * dependencies of makefiles.
   */
  void CleanupFiles(std::string const& binDir);

  /**
   * This tries to find the (executable) file created by
  TryCompileCode. The result is stored in OutputFile. If nothing is found,
  the error message is stored in FindErrorMessage.
   */
  void FindOutputFile(const std::string& targetName,
                      cmStateEnums::TargetType targetType);

  std::string BinaryDirectory;
  std::string OutputFile;
  std::string FindErrorMessage;
  bool SrcFileSignature = false;
  cmMakefile* Makefile;
};
