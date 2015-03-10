// This file is part of korektor <http://github.com/ufal/korektor/>.
//
// Copyright 2015 by Institute of Formal and Applied Linguistics, Faculty
// of Mathematics and Physics, Charles University in Prague, Czech Republic.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under 3-clause BSD licence.

#include "common.h"
#include "lexicon/lexicon.h"
#include "spellchecker/configuration.h"
#include "spellchecker/spellchecker.h"
#include "token/input_format.h"
#include "token/output_format.h"
#include "utils/options.h"
#include "utils/parse.h"
#include "version/version.h"

using namespace ufal::korektor;

int main(int argc, char* argv[]) {
  iostream::sync_with_stdio(false);

  Options::Map options;
  if (!Options::Parse({{"input",Options::Value{"untokenized", "untokenized_lines", "segmented", "vertical", "horizontal"}},
                      {"output",Options::Value{"original", "xml", "vertical", "horizontal"}},
                      {"corrections",Options::Value::any},
                      {"context_free", Options::Value::none},
                      {"version", Options::Value::none},
                      {"help", Options::Value::none}}, argc, argv, options) ||
      options.count("help") ||
      (argc < 2 && !options.count("version")))
    runtime_failure("Usage: " << argv[0] << " [options] model_configuration\n"
                    "Options: --input=untokenized|untokenized_lines|segmented|vertical|horizontal\n"
                    "         --output=original|xml|vertical|horizontal\n"
                    "         --corrections=maximum_number_of_corrections\n"
                    "         --context_free\n"
                    "         --version\n"
                    "         --help");
  if (options.count("version"))
    runtime_failure(version::version_and_copyright());

  // Init options
  unsigned max_corrections = 0;
  if (options.count("corrections")) {
    max_corrections = Parse::Int(options["corrections"], "maximum number of corrections");
    if (max_corrections <= 0)
      runtime_failure("The maximum number of corrections (" << max_corrections << ") must be at least 1!");
  }

  bool context_free = options.count("context_free");

  // Load spellchecker
  ConfigurationP configuration(new Configuration(argv[1]));
  Spellchecker spellchecker = Spellchecker(configuration.get());

  if (!max_corrections) {
    if (configuration->mode_string == "autocorrect")
      max_corrections = 1;
    else if (configuration->mode_string == "tag_errors")
      max_corrections = 5;
    else
      runtime_failure("Unknown mode '" << configuration->mode_string << "' in the configuration file '" << argv[1] << "'!");
  }

  // Init input format
  string input_format_name = options.count("input") ? options["input"] : "untokenized";
  auto input_format = InputFormat::NewInputFormat(input_format_name, configuration->lexicon);
  if (!input_format)
    runtime_failure("Cannot create input format '" << input_format_name << "'.");

  // Init output format
  string output_format_name = options.count("output") ? options["output"] : max_corrections == 1 ? "original" : "xml";
  auto output_format = OutputFormat::NewOutputFormat(output_format_name);
  if (!output_format)
    runtime_failure("Cannot create output format '" << output_format_name << "'.");

  // Check that output format supports multiple corrections if required
  if (max_corrections > 1 && !output_format->CanHandleAlternatives())
    runtime_failure("Output format '" << output_format_name << "' cannot handle multiple corrections!");

  // Process data
  string input_block, output_block;
  vector<TokenP> tokens;
  vector<SpellcheckerCorrection> corrections;
  while (input_format->ReadBlock(cin, input_block)) {
    input_format->SetBlock(input_block);
    output_format->SetBlock(input_block);
    output_block.clear();

    // For all sentences in the input block
    while (input_format->NextSentence(tokens)) {
      // Perform the spellchecking
      if (!context_free) {
        spellchecker.Spellcheck(tokens, corrections, max_corrections - 1);
      } else {
        corrections.resize(tokens.size());
        for (unsigned i = 0; i < tokens.size(); i++)
          spellchecker.SpellcheckToken(tokens[i], corrections[i], max_corrections - 1);
      }

      // Output the results
      output_format->AppendSentence(output_block, tokens, corrections);
    }

    // Write the output block
    output_format->FinishBlock(output_block);
    cout << output_block;
  }

  return 0;
}