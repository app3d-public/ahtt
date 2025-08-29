const fs = require("fs");
const path = require("path");
const pugLexer = require("pug-lexer");
const pugParser = require("pug-parser");
const ejs = require("ejs");

function parseValue(input, dst) {
    const regex = /(_\(([^)]+)\))/g;
    let lastIndex = 0;
    let match;

    while ((match = regex.exec(input)) !== null) {
        if (match.index > lastIndex)
            dst.push({ value: input.slice(lastIndex, match.index), type: "text" });

        dst.push({ value: `_("${match[2].trim()}")`, type: "locale" });
        lastIndex = regex.lastIndex;
    }

    if (lastIndex < input.length)
        dst.push({ value: input.slice(lastIndex), type: "text" });
}

function extractBlocks(node, blocks) {
    if (!node) return;

    if (node.type === "Block") {
        (node.nodes || []).forEach(child => extractBlocks(child, blocks));
    }
    else if (node.type === "Tag") {
        blocks.push({ value: `<${node.name}`, type: "text" });
        if (node.attrs && node.attrs.length > 0) {
            node.attrs.forEach(attr => {
                blocks.push({ value: ` ${attr.name}=`, type: "text" });
                parseValue(attr.val, blocks);
            });
        }
        blocks.push({ value: ">", type: "text" });

        if (node.block) {
            extractBlocks(node.block, blocks);
        }

        blocks.push({ value: `</${node.name}>`, type: "text" });
    }
    else if (node.type == "Doctype") {
        blocks.push({ value: `<!DOCTYPE ${node.val}>`, type: "text" });
    }
    else if (node.type === "Text") {
        parseValue(node.val, blocks);
    }
    else if (node.type === "Code" && node.buffer) {
        blocks.push({ value: node.val, type: "variable" });
    }
    else if (node.type === "Extends") {
        blocks.push({ value: node.file.path, type: "extends" });
    }
    else if (node.type === "NamedBlock") {
        const content = [];
        if (node.block) extractBlocks(node.block, content);
        if (node.nodes && node.nodes.length) {
            node.nodes.forEach(child => extractBlocks(child, content));
        }
        blocks.push({ value: node.name, type: "block", content: optimizeBlocks(content) });
    }
}

function optimizeBlocks(blocks) {
    const optimized = [];
    let buffer = "";

    for (const block of blocks) {
        if (block.type === "text") buffer += block.value;
        else {
            if (buffer) {
                optimized.push({ value: buffer, type: "text" });
                buffer = "";
            }
            optimized.push(block);
        }
    }

    if (buffer)
        optimized.push({ value: buffer, type: "text" });
    return optimized;
}

function generateAST(inputFile) {
    if (!fs.existsSync(inputFile)) {
        console.error(`Error. File not found: ${inputFile}`);
        process.exit(1);
    }

    const pugSource = fs.readFileSync(inputFile, "utf8");
    const tokens = pugLexer(pugSource);
    const ast = pugParser(tokens);

    const blocks = [];
    extractBlocks(ast, blocks);
    return optimizeBlocks(blocks);
}

const args = process.argv.slice(2);
if (args.length < 2) {
    console.error("Usage: npm start -- input.pug output_dir");
    process.exit(1);
}

const blocks = generateAST(args[0]);
console.dir(blocks, { depth: null, colors: true, maxArrayLength: null });
const build_params = {
    json: blocks,
    variables: [...new Set(blocks.filter(e => e.type === "variable").map(e => e.value))],
    extendPath: (blocks.find(e => e.type === "extends") || {}).value || null,
    blocks: blocks.filter(e => e.type === "block"),
    namespace: path.parse(args[0]).name
}
const headerTemplate = fs.readFileSync("templates/header.ejs", "utf8");
const headerCode = ejs.render(headerTemplate, build_params);
fs.writeFileSync(`${args[1]}/${build_params.namespace}.hpp`, headerCode);

const implTemplate = fs.readFileSync("templates/impl.ejs", "utf8");
const implCode = ejs.render(implTemplate, build_params);
fs.writeFileSync(`${args[1]}/${build_params.namespace}.cpp`, implCode);