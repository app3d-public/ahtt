#include <acul/log.hpp>
#include <args.hxx>
#include <version.h>
#include "linker.hpp"
#include "translator.hpp"

#define AHTT_ARGS_ERR     -1
#define AHTT_ARGS_SUCCESS 0
#define AHTT_ARGS_HELP    1
#define AHTT_ARGS_VERSION 2

struct Args
{
    acul::io::path input;
    acul::string output;
    acul::io::path base_dir;
};

void print_version() { std::cout << "ahtt version " << AHTT_VERSION_STRING << "\n"; }

int parse_args(int argc, char **argv, Args &args)
{
    args::ArgumentParser parser("ahtt " AHTT_VERSION_STRING);
    args::HelpFlag help(parser, "help", "Show help", {'h', "help"});
    args::Flag version(parser, "version", "Show version", {'v', "version"}, args::Options::KickOut);

    args::ValueFlag<std::string> input(parser, "file", "Input .at template", {'i', "input"}, args::Options::Required);
    args::ValueFlag<std::string> output(parser, "dir", "Output directory for generated .cpp/.hpp", {'o', "output"},
                                        args::Options::Required);
    args::ValueFlag<std::string> base_dir(parser, "dir", "Base directory", {"base-dir"});

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Help &)
    {
        std::cout << parser;
        return AHTT_ARGS_HELP;
    }
    catch (const args::ParseError &e)
    {
        std::cerr << e.what() << "\n" << parser;
        return AHTT_ARGS_ERR;
    }
    catch (const args::ValidationError &e)
    {
        std::cerr << e.what() << "\n" << parser;
        return AHTT_ARGS_ERR;
    }
    catch (const args::Completion &c)
    {
        std::cout << c.what();
        return AHTT_ARGS_HELP;
    }
    catch (...)
    {
        std::cerr << "Unknown argument error\n";
        return AHTT_ARGS_ERR;
    }

    if (version)
    {
        print_version();
        return AHTT_ARGS_VERSION;
    }

    args.input = acul::string(args::get(input).c_str());
    args.output = acul::string(args::get(output).c_str());
    if (base_dir) args.base_dir = acul::string(args::get(base_dir).c_str());
    return AHTT_ARGS_SUCCESS;
}

int main(int argc, char *argv[])
{
    Args args;
    switch (parse_args(argc, argv, args))
    {
        case AHTT_ARGS_HELP:
        case AHTT_ARGS_VERSION:
            return 0;
        case AHTT_ARGS_ERR:
            return 1;
        default:
            break;
    }

    acul::task::service_dispatch sd;
    sd.run();
    auto *ls = acul::alloc<acul::log::log_service>();
    sd.register_service(ls);
    ls->level = acul::log::level::trace;
    auto *app_logger = ls->add_logger<acul::log::console_logger>("console");
    ls->default_logger = app_logger;
    app_logger->set_pattern("[%(level_name)] %(message)\n");
    ls->default_logger = app_logger;

    acul::init_simd_module();
    if (acul::get_simd_flags() == acul::simd_flag_bits::initialized) LOG_WARN("Failed to load simd module\n");

    LOG_INFO("Translating template: %s", args.input.str().c_str());
    int ret = EXIT_SUCCESS;
    try
    {
        ahtt::Parser p;
        ahtt::load_template(args.base_dir / args.input, p);
        ahtt::Linker l(p);
        l.link(args.base_dir);
        ahtt::Translator tr(p);
        tr.parse_tokens();
        acul::stringstream ss;
        tr.write_to_stream(ss, args.input.stem());
        LOG_INFO("Writing to %s", args.output.c_str());

        auto file_content = ss.str();
        if (!acul::io::file::write_binary(args.output, file_content.data(), file_content.size()))
            throw acul::runtime_error(acul::format("Failed to write file: %s", args.output.c_str()));
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("%s", e.what());
        ret = EXIT_FAILURE;
    }
    ls->await();
    return ret;
}