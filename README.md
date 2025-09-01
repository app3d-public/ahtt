# Alwf HTML Template Translator

**apcc** is a pug-to-C++ code generator for [alwf](https://git.homedatasrv.ru/app3d/alwf).
It reads .pug templates and emits deterministic C++ files that you include and call from your alwf app.

> [!NOTE]
> This tool is purpose-built for `alwf`/`acul`. It is not a general-purpose Pug compiler.

## Pug Support Status

* **Base HTML tags:** tags & nesting, static attributes, and text nodes.
* **Variables:**
    * Text/attribute values: simple placeholders parsed by the compiler.
    * Pug Code nodes: buffered output only.
* **Layout composition:** `extends` and `block`.
* **i18n:** emitted text segments can be translated via gettext integration in `acul`.

## Building

### Requirements
* Node.js ≥ 18 (LTS recommended)
* C++20 compiler

### Dependencies
Runtime/build deps installed via npm install:
* pug
* pug-lexer
* pug-parser
* ejs

### Build
```sh
npm install
npm start -- input.pug output_dir
```

## License
This project is licensed under the [MIT License](LICENSE).

## Contacts
For any questions or feedback, you can reach out via [email](mailto:wusikijeronii@gmail.com) or open a new issue.