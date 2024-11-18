import os
import re
import shutil
import subprocess
import sys

project_dir = os.path.dirname(os.path.abspath(__file__))
generated_dir = os.path.join(project_dir, 'source/shader/glsl_generated')

dxc_path = os.path.join(project_dir, 'bin/dxc')
spriv_cross_path = os.path.join(project_dir, 'bin/spirv-cross')

output_spv = 'tmp.spv'

input_shaders = [
    { 'path': 'shadowmap_point.vert', 'animated': True, },
    { 'path': 'shadowmap_point.pixel', 'animated': False, },
    { 'path': 'bloom_setup.comp', 'animated': False, },
    { 'path': 'bloom_downsample.comp', 'animated': False, },
    { 'path': 'bloom_upsample.comp', 'animated': False, },
    { 'path': 'particle_initialization.comp', 'animated': False, },
    { 'path': 'particle_kickoff.comp', 'animated': False, },
    { 'path': 'particle_emission.comp', 'animated': False, },
    { 'path': 'particle_simulation.comp', 'animated': False, },
]

def insert_file_name(file_path):
    if not os.path.isfile(file_path):
        return

    with open(file_path, 'r') as file:
        content = file.readlines()

    file_name = os.path.basename(file_path)
    content.insert(0, f'/// File: {file_name}\n')

    with open(file_path, 'w') as file:
        file.writelines(content)

    return

def generate(hlsl_source, animated):
    full_input_path = os.path.join(
        project_dir,
        f'source/shader/hlsl/{hlsl_source}.hlsl')
    # shader model
    shader_model = ''
    if hlsl_source.endswith('.vert'):
        shader_model = 'vs_6_0'
    elif hlsl_source.endswith('.pixel'):
        shader_model = 'ps_6_0'
    elif hlsl_source.endswith('.comp'):
        shader_model = 'cs_6_0'
    else:
        raise RuntimeError(f'Unknown shader type "{full_input_path}"')

    # output path
    glsl_file = f'{generated_dir}/{hlsl_source}.glsl'

    # generate shader
    include_path = os.path.join(project_dir, 'source/shader/')

    spv_command = [dxc_path, full_input_path, '-T', shader_model, '-E', 'main', '-Fo', output_spv, '-spirv', '-I', include_path, '-D HLSL_LANG=1', '-D HLSL_LANG_D3D11=1']
    generate_files = [ { 'filename': glsl_file, 'command' : spv_command } ]
    if animated:
        new_command = spv_command + ['-D HAS_ANIMATION=1']
        if not hlsl_source.endswith('.vert'):
            raise RuntimeError(f'"{full_input_path}" is not vertex shader')
        new_path = f'source/shader/glsl_generated/animated_{hlsl_source}.glsl'
        generate_files.append({ 'filename': new_path, 'command': new_command })

    for generate_file in generate_files:
        generated_file_name = generate_file['filename']
        proc = subprocess.run(generate_file['command'])
        if (proc.returncode != 0):
            print(f'running: {' '.join(generate_file['command'])}')
            raise RuntimeError(f'Failed to generate "{generated_file_name}"')
        proc = subprocess.run([spriv_cross_path, output_spv, '--version', '450', '--output', generated_file_name])
        if (proc.returncode != 0):
            print(f'running: {' '.join(generate_file['command'])}')
            raise RuntimeError(f'Failed to generate "{generated_file_name}"')
        print(f'file "{generated_file_name}" generated')
        insert_file_name(generated_file_name)
    return

def delete_and_create_folder(folder):
    if os.path.exists(folder):
        print(f'cleaning folder {folder}')
        shutil.rmtree(folder)
    os.makedirs(folder)

def func_generate_files():
    # remove generated path
    delete_and_create_folder(generated_dir)

    for l in input_shaders:
        generate(l['path'], l['animated'])

def func_insert_names():
    for root, _, files in os.walk('source/shader', topdown=False):
        for name in files:
            file_path = os.path.join(root, name)
            _, file_ext = os.path.splitext(file_path)
            if file_ext in ['.h', '.hlsl', '.glsl']:
                print(f'*** formatting file {file_path}')
                insert_file_name(file_path)

try:
    func_generate_files()

except RuntimeError as e:
    print(f'RuntimeError: {e}')
finally:
    # delete .spv
    if (os.path.isfile(output_spv)):
        os.remove(output_spv)
