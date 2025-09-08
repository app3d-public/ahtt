# Alwf HTML Template Transpiler

**ahtt** is a transpiler that converts `.at` template files into C++ source code.
`.at` is a template format with a Pug-like syntax for describing HTML views.
The transpiler produces deterministic C++ functions that can be included and called directly from an application, removing the need for runtime template processing.

Specification of the `.at` template format is available in the [project wiki](https://github.com/app3d-public/ahtt/wiki).

## Template Features

* **Base HTML tags:** nesting, static attributes, text nodes
* **Variables:** placeholders expanded by the compiler in text and attributes
* **Code nodes:** buffered output only
* **Layout composition:** `extends`, `block`
* **i18n:** emitted text segments integrate with `acul` gettext support

## Building

### Requirements

* C++20 compiler

### Supported compilers:
- GNU GCC
- Clang

### Supported OS:
- Linux
- Microsoft Windows

### Bundled submodules
* [acul](https://github.com/app3d-public/acul)
* [args](https://github.com/Taywee/args)

## Usage

```sh
ahtt -i input.at -o output_dir [--base-dir path]
```

### Options
### Required
* `-i, --input` - input `.at` template
* `-o, --output` - output .hpp file
### Optional
* `--base-dir` - base directory for resolving templates
* `--dep-file` - output dependency file (Cmake)
* `--help` - show usage information
* `--version` - show version information

## License
This project is licensed under the [MIT License](LICENSE).

## Contacts
For any questions or feedback, you can reach out via [email](mailto:wusikijeronii@gmail.com) or open a new issue.