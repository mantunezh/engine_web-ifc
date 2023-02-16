const fs = require("fs");

let pckg = JSON.parse(fs.readFileSync("package.json").toString());

console.log(`Setting version to ${pckg.version}`);

let file = `
#pragma once\n#include<string>\n
const std::string WEB_IFC_VERSION_NUMBER = "${pckg.version}";
`

fs.writeFileSync("./src/wasm/version.h", file);