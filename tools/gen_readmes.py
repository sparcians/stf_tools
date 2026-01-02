#!/usr/bin/env python3

import os
from pathlib import Path
import re
import subprocess
import sys
import yaml

README_TEMPLATE = \
"""# Table of Contents
{toc}
{categories}
"""

CATEGORY_TEMPLATE = \
"""
---

# {category}
{description}

{tool_list}"""

TOOL_README_TEMPLATE = \
"""# {tool_name}

{description}
{note}
```
{usage}
```
"""

NOTE_TEMPLATE = \
"""
> [!NOTE]
> {note}
"""

def format_list_link(name, target):
    return f'- [{name}]({target})'

def format_toc_entry(toc_name):
    anchor = '#' + re.sub(r'\s+', '-', toc_name.lower())
    return format_list_link(toc_name, anchor)

def format_category(category_info, tool_list):
    return CATEGORY_TEMPLATE.format(
        category=category_info['toc_name'],
        description=category_info['description'],
        tool_list='\n'.join(format_list_link(tool, f'{tool}/README.md') for tool in sorted(tool_list)))

def read_yaml(file):
    with open(file, 'r') as f:
        config = yaml.safe_load(f)
    return config

if len(sys.argv) != 3:
    print('Usage: gen_readme.py <tools binary directory> <tools source directory>')
    sys.exit(1)

tools_binary_dir = Path(sys.argv[1])
tools_source_dir = Path(sys.argv[2])

category_yaml = tools_source_dir / 'readme.yaml'

if not category_yaml.is_file():
    print(f'Could not find readme.yaml in {tools_source_dir}')
    sys.exit(1)

category_config = read_yaml(category_yaml)

extra_tools_yaml = os.environ.get('STF_EXTRA_TOOLS_YAML')

if extra_tools_yaml:
    if not category_yaml.is_file():
        print(f'Could not find extra tools yaml {extra_tools_yaml}')
        sys.exit(1)

    category_config.extend(read_yaml(extra_tools_yaml))

categorized = {v['category']: [] for v in category_config}

# Write each tool's README.md
for child in tools_binary_dir.iterdir():
    if child.is_dir():
        tool_name = child.name
        if tool_name != 'CMakeFiles':
            tool_binary = child / tool_name
            if not tool_binary.is_file():
                print(f'Could not find compiled binary for {tool_name}. Run make before running make readmes')
                sys.exit(1)

            tool_source_dir = tools_source_dir / tool_name

            conf_yaml = tool_source_dir / 'readme.yaml'

            if not conf_yaml.is_file():
                print(f'Could not find readme.yaml in tool directory {tool_source_dir}')
                sys.exit(1)

            config = read_yaml(conf_yaml)

            category = config['category']

            if category not in categorized:
                print(f'Unknown tool category: {category}')
                sys.exit(1)

            categorized[category].append(tool_name)

            readme = tool_source_dir / 'README.md'

            if 'note' in config:
                note = NOTE_TEMPLATE.format(note=config['note'])
            else:
                note = ''

            with open(readme, 'w') as f:
                usage_result = subprocess.run([tool_binary, '-h'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

                f.write(TOOL_README_TEMPLATE.format(
                    tool_name=tool_name,
                    description=config['description'],
                    note=note,
                    usage=usage_result.stdout.decode(encoding="utf-8").rstrip()
                ))

# Write tools/README.md
with open(tools_source_dir / 'README.md', 'w') as f:
    f.write(README_TEMPLATE.format(
        toc='\n'.join(format_toc_entry(category['toc_name']) for category in category_config),
        categories = '\n'.join(format_category(category, categorized[category['category']]) for category in category_config)
    ))
