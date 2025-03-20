#include "cli.h"
#include "loopscheduler.h"
#include "clilocalsession.h"
#include "clifilesession.h"
#include "../rpc/ad_rpc.h"
#include "../rpc/gen_code/cpp/config_management.h"
#include "sm_config.h"
#include "device_config.h"
#include "runtime_config.h"
#include "redis_config.h"

static std::string insert_spaces(const std::string &_str)
{
    std::stringstream ss(_str);
    std::string line;
    std::string result;
    while (std::getline(ss, line))
    {
        result += "  " + line + "\n";
    }
    return result;
}

int un_safe_main(int argc, char const *argv[])
{
    AD_LOGGER::set_global_log_level(AD_LOGGER::ERROR);
    common_cli *sub_c[] = {
        new DEVICE_CONFIG_CLI(), new SM_CONFIG_CLI(), new RUNTIME_CONFIG_CLI(), new REDIS_CONFIG_CLI()};
    auto root_menu = std::unique_ptr<cli::Menu>(new cli::Menu("ad"));
    for (auto &itr : sub_c)
    {
        root_menu->Insert(std::move(itr->menu));
    }

    auto make_bdr_string = [&]()
    {
        std::string bdr_str;
        for (auto &itr : sub_c)
        {
            bdr_str += itr->menu_name + "\n";
            bdr_str += insert_spaces(itr->make_bdr());
            bdr_str += "ad\n";
        }
        return bdr_str;
    };

    root_menu->Insert(
        "bdr",
        [&](std::ostream &_out)
        {
            _out << make_bdr_string();
        });
    root_menu->Insert(
        "clear",
        [&](std::ostream &_out)
        {
            for (auto &itr : sub_c)
            {
                itr->clear();
            }
        });
    root_menu->Insert(
        "save",
        [&](std::ostream &_out)
        {
            std::ofstream(AD_CONST_SAVED_CONFIG_FILE, std::ios::trunc) << make_bdr_string();
        });
    root_menu->Insert(
        "bash",
        [&](std::ostream &_out)
        {
            constexpr tcflag_t ICANON_FLAG = ICANON;
            constexpr tcflag_t ECHO_FLAG = ECHO;

            termios oldt;
            termios newt;
            tcgetattr(STDIN_FILENO, &oldt);
            newt = oldt;
            newt.c_lflag |= (ICANON_FLAG | ECHO_FLAG);
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
            system("/bin/bash");
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        });
    cli::LoopScheduler *plsc = nullptr;
    cli::Cli cli(std::move(root_menu));
    cli.ExitAction(
        [&](std::ostream &out)
        {
            AD_RPC_SC::get_instance()->stop_server();
            if (plsc)
            {
                plsc->Stop();
            }
        });

    auto sc = AD_RPC_SC::get_instance();
    AD_CO_ROUTINE_PTR cli_co;
    if (argc == 1)
    {
        cli::LoopScheduler lsc;
        cli::CliLocalTerminalSession ss(cli, lsc, std::cout);
        plsc = &lsc;
        cli_co = sc->add_co(
            [&]()
            {
                lsc.Run(ss);
            });

        sc->resume_co(cli_co);

        sc->start_server();
    }
    else
    {
        std::fstream cmd_file(argv[1], std::ios::in);
        cli::CliFileSession cf(cli, cmd_file);
        cli_co = sc->add_co(
            [&]()
            {
                cf.Start();
            });

        sc->resume_co(cli_co);

        sc->start_server();
    }

    return 0;
}
#include <iostream>
#include <security/pam_appl.h>
#include <security/pam_misc.h>

// PAM会话句柄
pam_handle_t *pamHandle = nullptr;

// 回调函数，用于获取用户名和密码
int conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
    if (num_msg != 1)
        return PAM_CONV_ERR;

    const char *username = "root"; // 要认证的用户名
    std::string password = "";     // 用户密码
    std::cout << "Enter password: ";
    std::cin >> password;

    struct pam_response *responses = (struct pam_response *)malloc(sizeof(struct pam_response) * num_msg);
    if (responses == nullptr)
        return PAM_BUF_ERR;

    responses[0].resp = strdup(password.c_str());
    responses[0].resp_retcode = 0;
    *resp = responses;

    return PAM_SUCCESS;
}

// 主函数
int main(int argc, char const *argv[])
{
    if (argc == 1)
    {
        // 定义PAM会话的对话方式
        struct pam_conv pamConversation = {conversation, nullptr};

        // 初始化PAM会话
        int retval = pam_start("login", "root", &pamConversation, &pamHandle);
        if (retval != PAM_SUCCESS)
        {
            std::cerr << "PAM start error: " << pam_strerror(pamHandle, retval) << std::endl;
            return 1;
        }

        // 尝试用户身份认证
        retval = pam_authenticate(pamHandle, 0);
        if (retval != PAM_SUCCESS)
        {
            std::cerr << "PAM authentication error: " << pam_strerror(pamHandle, retval) << std::endl;
            pam_end(pamHandle, retval);
            return 1;
        }

        // 验证用户账户
        retval = pam_acct_mgmt(pamHandle, 0);
        if (retval != PAM_SUCCESS)
        {
            std::cerr << "PAM account management error: " << pam_strerror(pamHandle, retval) << std::endl;
            pam_end(pamHandle, retval);
            return 1;
        }

        // 认证成功，执行后续代码
        std::cout << "Authentication successful. Running post-login code..." << std::endl;

        // 在这里放置你的后续代码
        un_safe_main(argc, argv);
        // 结束PAM会话
        pam_end(pamHandle, PAM_SUCCESS);
    }
    else
    {
        un_safe_main(argc, argv);
    }

    return 0;
}
