#pragma once

#include <string>
#include <vector>

#include "sir/module.h"

namespace seq {

std::shared_ptr<ir::SIRModule> parse(const std::string &argv0, const std::string &file,
                                     const std::string &code = "", bool isCode = false,
                                     bool isTest = false, int startLine = 0);
// void execute(seq::SeqModule *module, std::vector<std::string> args = {},
//             std::vector<std::string> libs = {}, bool debug = false);

void compile(std::shared_ptr<ir::SIRModule>, const std::string &out,
             bool debug = false);
void generateDocstr(const std::string &file);

} // namespace seq
