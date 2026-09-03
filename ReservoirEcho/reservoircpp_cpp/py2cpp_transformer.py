import re
import os
import ast
import astor  # For parsing Python AST


class Py2CppTransformer:
    """
    A simple Python to C++ code transformer.
    This handles basic syntax conversion for common patterns.
    """
    
    def __init__(self):
        self.python_types_to_cpp = {
            'int': 'int',
            'float': 'double',
            'bool': 'bool',
            'str': 'std::string',
            'list': 'std::vector',
            'dict': 'std::unordered_map',
            'set': 'std::unordered_set',
            'tuple': 'std::tuple',
            'None': 'nullptr'
        }
        
        self.python_funcs_to_cpp = {
            'len': 'size',
            'print': 'std::cout',
            'range': 'range',  # Will need custom implementation
            'enumerate': 'enumerate'  # Will need custom implementation
        }
        
        # Common Python libraries and their C++ equivalents
        self.lib_imports = {
            'numpy': ['Eigen/Dense', 'Eigen/Sparse', 'reservoircpp/numpy.hpp'],
            'scipy': ['reservoircpp/scipy.hpp'],
            'random': ['random'],
            'logging': ['iostream', 'reservoircpp/logging.hpp'],
            'os': ['filesystem'],
            'time': ['chrono']
        }
        
    def preprocess_comments(self, code):
        """Convert Python docstrings and comments to C++ style"""
        # Convert triple quoted docstrings to C++ style multiline comments
        code = re.sub(r'"""(.*?)"""', r'/**\n\1\n*/', code, flags=re.DOTALL)
        code = re.sub(r"'''(.*?)'''", r'/**\n\1\n*/', code, flags=re.DOTALL)
        
        # Convert pound/hash comments to C++ style
        lines = code.split('\n')
        for i in range(len(lines)):
            if '#' in lines[i]:
                comment_start = lines[i].find('#')
                code_part = lines[i][:comment_start]
                comment_part = lines[i][comment_start+1:]
                lines[i] = f"{code_part}// {comment_part}"
        
        return '\n'.join(lines)
    
    def transform_imports(self, code):
        """Transform Python imports to C++ includes"""
        imports = []
        import_re = re.compile(r'^(?:from\s+(\S+)\s+)?import\s+(.+?)(?:\s+as\s+(\S+))?$', re.MULTILINE)
        
        for match in import_re.finditer(code):
            module = match.group(1) or ''
            names = match.group(2).strip()
            alias = match.group(3)
            
            if module in self.lib_imports:
                # For known libraries, add their C++ equivalents
                for header in self.lib_imports[module]:
                    imports.append(f'#include <{header}>')
                    
            elif module:
                # For project modules, include corresponding header
                module_path = module.replace('.', '/')
                if names == '*':
                    imports.append(f'#include "reservoircpp/{module_path}.hpp"')
                else:
                    for item in names.split(','):
                        item = item.strip()
                        imports.append(f'#include "reservoircpp/{module_path}/{item}.hpp"')
            else:
                # Direct imports become headers
                for item in names.split(','):
                    item = item.strip()
                    imports.append(f'#include "{item}.hpp"')
        
        # Remove import lines
        code = import_re.sub('', code)
        
        # Add includes at the top of the file
        imports = list(set(imports))  # Remove duplicates
        includes = '\n'.join(imports) + '\n\n'
        
        # Add namespace usage as needed
        includes += 'using namespace reservoircpp;\n'
        includes += 'using namespace Eigen;\n\n'
        
        return includes + code
    
    def transform_functions(self, code):
        """Transform Python functions to C++ functions"""
        function_re = re.compile(r'def\s+(\w+)\s*\((.*?)\)(?:\s*->\s*(\w+))?\s*:', re.DOTALL)
        
        def transform_function(match):
            name = match.group(1)
            params = match.group(2)
            return_type = match.group(3)
            
            # Transform parameter list
            param_list = []
            if params:
                for param in params.split(','):
                    param = param.strip()
                    if '=' in param:
                        name, default = param.split('=', 1)
                        name = name.strip()
                        default = default.strip()
                        
                        # Try to infer type or use auto
                        param_type = 'auto'
                        if default.isdigit():
                            param_type = 'int'
                        elif default.replace('.', '', 1).isdigit():
                            param_type = 'double'
                        elif default == 'True' or default == 'False':
                            param_type = 'bool'
                            default = default.lower()
                        elif default.startswith('"') or default.startswith("'"):
                            param_type = 'std::string'
                            
                        param_list.append(f"{param_type} {name} = {default}")
                    elif ':' in param:
                        name, type_hint = param.split(':', 1)
                        name = name.strip()
                        type_hint = type_hint.strip()
                        cpp_type = self.python_types_to_cpp.get(type_hint, 'auto')
                        param_list.append(f"{cpp_type} {name}")
                    else:
                        param_list.append(f"auto {param}")
                        
            cpp_params = ', '.join(param_list)
            
            # Determine return type
            cpp_return_type = 'auto'
            if return_type:
                cpp_return_type = self.python_types_to_cpp.get(return_type, 'auto')
                
            return f"{cpp_return_type} {name}({cpp_params}) {{"
        
        return function_re.sub(transform_function, code)
    
    def transform_classes(self, code):
        """Transform Python classes to C++ classes"""
        class_re = re.compile(r'class\s+(\w+)(?:\s*\(([^)]*)\))?\s*:', re.DOTALL)
        
        def transform_class(match):
            class_name = match.group(1)
            inheritance = match.group(2) or ''
            
            base_classes = []
            if inheritance:
                for base in inheritance.split(','):
                    base = base.strip()
                    if base:
                        base_classes.append(f"public {base}")
            
            inheritance_str = ": " + ", ".join(base_classes) if base_classes else ""
            return f"class {class_name}{inheritance_str} {{"
        
        code = class_re.sub(transform_class, code)
        
        # Add public/private sections
        method_re = re.compile(r'^(\s+)def\s+(\w+)', re.MULTILINE)
        return method_re.sub(r'\1public:\n\1auto \2', code)
    
    def transform_control_structures(self, code):
        """Transform Python control structures to C++"""
        # Replace if statements
        code = re.sub(r'if\s+(.*?):', r'if (\1) {', code)
        code = re.sub(r'elif\s+(.*?):', r'} else if (\1) {', code)
        code = re.sub(r'else:', r'} else {', code)
        
        # Replace for loops
        code = re.sub(r'for\s+(.*?)\s+in\s+(.*?):', r'for (auto \1 : \2) {', code)
        
        # Replace while loops
        code = re.sub(r'while\s+(.*?):', r'while (\1) {', code)
        
        # Replace function ends with closing braces
        indentation_pattern = re.compile(r'^( *)def', re.MULTILINE)
        indentations = [len(m.group(1)) for m in indentation_pattern.finditer(code)]
        
        if indentations:
            # Find all points where the indentation decreases
            lines = code.split('\n')
            new_lines = []
            current_indent = 0
            
            for line in lines:
                leading_spaces = len(line) - len(line.lstrip(' '))
                
                if leading_spaces < current_indent:
                    # Close blocks when indentation decreases
                    levels_to_close = (current_indent - leading_spaces) // 4
                    for _ in range(levels_to_close):
                        indent = ' ' * (current_indent - 4)
                        new_lines.append(f"{indent}}}")
                
                new_lines.append(line)
                current_indent = leading_spaces
            
            # Close any remaining open blocks at the end
            while current_indent > 0:
                current_indent -= 4
                indent = ' ' * current_indent
                new_lines.append(f"{indent}}}")
                
            code = '\n'.join(new_lines)
        
        return code
    
    def transform_variables(self, code):
        """Transform Python variables to C++ variables with type inference"""
        # Simple type inference for variable declarations
        assignment_re = re.compile(r'(\s*)(\w+)\s*=\s*(.+)$', re.MULTILINE)
        
        def infer_type(value):
            value = value.strip()
            if value.isdigit():
                return 'int'
            elif value.replace('.', '', 1).isdigit():
                return 'double'
            elif value == 'True' or value == 'False':
                return 'bool'
            elif value.startswith('"') or value.startswith("'"):
                return 'std::string'
            elif value.startswith('['):
                return 'auto'  # List -> std::vector, but using auto for now
            elif value.startswith('{'):
                return 'auto'  # Dict -> std::unordered_map, but using auto for now
            elif value.startswith('('):
                return 'auto'  # Tuple -> std::tuple, but using auto for now
            else:
                return 'auto'
        
        def transform_assignment(match):
            indent = match.group(1)
            var_name = match.group(2)
            value = match.group(3)
            
            # Skip class variables
            if var_name.startswith('self.'):
                return f"{indent}{var_name} = {value}"
            
            var_type = infer_type(value)
            if var_type == 'bool' and (value == 'True' or value == 'False'):
                value = value.lower()
                
            return f"{indent}{var_type} {var_name} = {value}"
        
        return assignment_re.sub(transform_assignment, code)
    
    def add_header_guards(self, code, filename):
        """Add C++ header guards"""
        header_name = os.path.splitext(os.path.basename(filename))[0].upper() + "_HPP"
        header_guard = f"""#ifndef RESERVOIRCPP_{header_name}
#define RESERVOIRCPP_{header_name}

{code}

#endif // RESERVOIRCPP_{header_name}
"""
        return header_guard
    
    def transform(self, code, filename):
        """Apply all transformations to the Python code"""
        # Skip if file is empty
        if not code.strip():
            return ""
            
        code = self.preprocess_comments(code)
        code = self.transform_imports(code)
        code = self.transform_functions(code)
        code = self.transform_classes(code)
        code = self.transform_variables(code)
        code = self.transform_control_structures(code)
        
        # Add header guards only for header files
        if filename.endswith('.hpp'):
            code = self.add_header_guards(code, filename)
            
        return code


def transform_file(input_file, output_file):
    """Transform a single Python file to C++"""
    with open(input_file, 'r', encoding='utf-8') as f:
        python_code = f.read()
    
    transformer = Py2CppTransformer()
    cpp_code = transformer.transform(python_code, output_file)
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(cpp_code)
    
    print(f"Transformed {input_file} → {output_file}")


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) != 3:
        print("Usage: python py2cpp_transformer.py input.py output.cpp")
        sys.exit(1)
        
    transform_file(sys.argv[1], sys.argv[2])