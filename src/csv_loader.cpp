#include "../include/csv_loader.h"
#include "../include/utils.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace CsvLoader {

static std::vector<std::string> SplitCsvLine(const std::string& line) {
    std::vector<std::string> result;
    std::istringstream stream(line);
    std::string cell;

    while (std::getline(stream, cell, ',')) {
        // Trim whitespace
        cell.erase(0, cell.find_first_not_of(" \t\r\n"));
        cell.erase(cell.find_last_not_of(" \t\r\n") + 1);
        result.push_back(cell);
    }

    return result;
}

static std::string GetCellValue(const std::vector<std::string>& headers,
                               const std::vector<std::string>& row,
                               const std::string& columnName) {
    for (size_t i = 0; i < headers.size() && i < row.size(); ++i) {
        std::string header = headers[i];
        std::transform(header.begin(), header.end(), header.begin(), ::tolower);

        std::string searchName = columnName;
        std::transform(searchName.begin(), searchName.end(), searchName.begin(), ::tolower);

        if (header == searchName) {
            return row[i];
        }
    }
    return "";
}

static std::vector<std::string> ParseCsvVector(const std::string& input) {
    std::vector<std::string> outv;
    if (input.empty()) return outv;

    std::istringstream iss(input);
    std::string item;
    while (std::getline(iss, item, ',')) {
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        if (!item.empty()) outv.push_back(item);
    }

    return outv;
}

bool LoadAccountsCsv(CsvCreds& out, const char* hintAccountOrUser, const char* typeHint) {
    auto file_exists = [](const char* path)->bool{
        DWORD a = GetFileAttributesA(path);
        return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
    };

    auto trim_copy = [](const std::string& s){
        size_t b=0,e=s.size();
        while(b<e && (s[b]==' '||s[b]=='\t'||s[b]=='\r'||s[b]=='\n')) ++b;
        while(e>b && (s[e-1]==' '||s[e-1]=='\t'||s[e-1]=='\r'||s[e-1]=='\n')) --e;
        return s.substr(b,e-b);
    };

    auto split_csv_line = [&](const std::string& line){
        std::vector<std::string> outv;
        std::string cur;
        bool inq=false;
        for(char c: line){
            if(c=='"'){inq=!inq; continue;}
            if(!inq && (c==','||c==';'||c=='\t')){
                outv.push_back(trim_copy(cur));
                cur.clear();
            } else cur.push_back(c);
        }
        outv.push_back(trim_copy(cur));
        return outv;
    };

    auto build_parent_history_path = [&](char* outp, size_t outsz, const char* fileName){
        char parent[MAX_PATH];
        strcpy_s(parent, G.DllPath);
        size_t len=strlen(parent);
        if(len>0 && (parent[len-1]=='\\'||parent[len-1]=='/')) parent[len-1]='\0';
        char* last=strrchr(parent,'\\');
        if(last) *last='\0';
        sprintf_s(outp,outsz, "%s\\History\\%s", parent, fileName);
    };

    char path1[MAX_PATH]; strcpy_s(path1, G.DllPath); strcat_s(path1, "Accounts.csv");
    char path2[MAX_PATH]; strcpy_s(path2, G.DllPath); strcat_s(path2, "account.csv");
    char path3[MAX_PATH]; build_parent_history_path(path3, sizeof(path3), "Accounts.csv");
    char path4[MAX_PATH]; build_parent_history_path(path4, sizeof(path4), "account.csv");

    const char* use = nullptr;
    if (file_exists(path1)) use = path1;
    else if (file_exists(path2)) use = path2;
    else if (file_exists(path3)) use = path3;
    else if (file_exists(path4)) use = path4;

    if (!use) {
        Utils::ShowMsg("Accounts.csv not found in plugin or History folder.");
        return false;
    }

    std::ifstream f(use);
    if (!f.is_open()) return false;

    std::string header;
    if (!std::getline(f, header)) return false;

    auto cols = split_csv_line(header);
    std::map<std::string,int> idx;
    for (size_t i=0;i<cols.size();++i){
        std::string k=cols[i];
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        idx[k]=(int)i;
    }

    auto get = [&](const std::vector<std::string>& row, const char* key)->std::string{
        std::string k(key);
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        auto it=idx.find(k);
        if(it==idx.end()||it->second>=(int)row.size()) return std::string();
        return row[it->second];
    };

    CsvCreds selected;
    bool found=false;

    while (f) {
        std::string line;
        if (!std::getline(f,line)) break;
        if(line.empty()) continue;
        auto row=split_csv_line(line);
        if(row.empty()) continue;

        std::string plugin = get(row, "Plugin");
        std::string server=get(row,"Server");
        std::string broker=get(row,"Broker");
        std::string accountName=get(row,"Account");
        std::string zorroName=get(row,"Name");
        std::string type=get(row,"Type");
        std::string realFlag=get(row,"Real");
        std::string accountId=get(row,"AccountId");
        if(accountId.empty()) accountId=get(row,"AccountNumber");
        std::string user=get(row,"User");
        if(user.empty()) user=get(row,"ClientId");
        std::string pass=get(row,"Pass");
        if(pass.empty()) pass=get(row,"ClientSecret");
        if(pass.empty()) pass=get(row,"Password");
        std::string token=get(row,"AccessToken");
        if(token.empty()) token=get(row,"Token");
        std::string redirectUri=get(row,"RedirectUri");
        if(redirectUri.empty()) redirectUri=get(row,"RedirectURL");
        std::string scope=get(row,"Scope");
        std::string product=get(row,"Product");

        auto contains_ci = [](const std::string& s, const char* sub){
            std::string a=s;
            std::string b=sub?sub:"";
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            std::transform(b.begin(), b.end(), b.begin(), ::tolower);
            return a.find(b)!=std::string::npos;
        };

        if (!plugin.empty() && !contains_ci(plugin.c_str(), "ctrader")) continue;
        if (!server.empty() && !contains_ci(server.c_str(), "ctrader")) continue;
        if (!broker.empty() && !contains_ci(broker.c_str(), "ctrader")) continue;

        bool matches=true;
        if(hintAccountOrUser && *hintAccountOrUser){
            bool digits=true;
            for(const char* p=hintAccountOrUser; *p; ++p){
                if(*p<'0'||*p>'9'){digits=false;break;}
            }
            matches = digits ? (accountId==hintAccountOrUser) :
                     (contains_ci(accountId.c_str(),hintAccountOrUser)||
                      contains_ci(user.c_str(),hintAccountOrUser)||
                      contains_ci(accountName.c_str(),hintAccountOrUser));
        }
        if(!matches) continue;

        bool typeOk=true;
        if(typeHint && *typeHint){
            if(!type.empty()) typeOk = contains_ci(type.c_str(), typeHint);
            else if(!zorroName.empty()) typeOk = contains_ci(zorroName.c_str(), typeHint);
            else if(!realFlag.empty()){
                int rf=atoi(realFlag.c_str());
                typeOk = (rf?contains_ci("live", typeHint):contains_ci("demo", typeHint));
            }
        }
        if(!typeOk) continue;

        CsvCreds candidate{};
        candidate.clientId = user;
        candidate.clientSecret = pass;
        candidate.accountId = accountId;
        candidate.type = !type.empty() ? type : (!zorroName.empty() ? zorroName : (atoi(realFlag.c_str()) ? "Live" : "Demo"));
        candidate.accessToken = token;
        candidate.server = server;

        if (!realFlag.empty()) {
            int rf = atoi(realFlag.c_str());
            candidate.hasExplicitEnv = true;
            candidate.explicitEnv = rf ? CtraderEnv::Live : CtraderEnv::Demo;
        }

        if(!candidate.clientId.empty() && !candidate.clientSecret.empty() && !candidate.accountId.empty()) {
            if (!redirectUri.empty()) {
                strncpy_s(G.RedirectUri, sizeof(G.RedirectUri), redirectUri.c_str(), _TRUNCATE);
            }
            if (!scope.empty()) {
                strncpy_s(G.Scope, sizeof(G.Scope), scope.c_str(), _TRUNCATE);
            }
            if (!product.empty()) {
                strncpy_s(G.Product, sizeof(G.Product), product.c_str(), _TRUNCATE);
            }

            selected = candidate;
            found=true;
            break;
        }
    }

    if(!found) return false;
    out=selected;
    char log[256];
    sprintf_s(log, "Loaded credentials from: %s", use);
    Utils::ShowMsg(log);
    return true;
}

}

