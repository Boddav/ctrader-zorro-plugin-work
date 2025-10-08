#pragma once
#include "globals.h"

namespace CsvLoader {
    bool LoadAccountsCsv(CsvCreds& out, const char* hintAccountOrUser, const char* typeHint);
}