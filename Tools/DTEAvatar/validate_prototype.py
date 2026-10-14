"""Blender structural validation. This does not replace native Unreal import QA."""
import bpy, json, sys, argparse, math, struct
from pathlib import Path
from mathutils import Vector
p=argparse.ArgumentParser(); p.add_argument('--assets',required=True); a=p.parse_args(sys.argv[sys.argv.index('--')+1:]); root=Path(a.assets).resolve()
bpy.ops.wm.open_mainfile(filepath=str(root/'DTE_Avatar_Prototype.blend'))
armatures=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']; meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
assert len(armatures)==1 and len(meshes)==1
arm=armatures[0]; assert len(arm.data.bones)==27
zero=[]; errors=[]
for o in meshes:
 for v in o.data.vertices:
  ws=[g.weight for g in v.groups if g.weight>0]
  if not ws: zero.append(v.index)
  assert all(math.isfinite(w) and 0<=w<=1 for w in ws)
  errors.append(abs(sum(ws)-1)); assert len(ws)<=3
assert not zero and max(errors)<1e-5
b= (root/'DTE_Avatar_Prototype.glb').read_bytes(); assert b[:4]==b'glTF'; n,t=struct.unpack_from('<II',b,12); j=json.loads(b[20:20+n])
assert len(j.get('skins',[]))==1 and len(j['skins'][0]['joints'])==27
assert j.get('animations'), 'No animation in exported GLB'
for m in j['meshes']:
 for q in m['primitives']: assert 'JOINTS_0' in q['attributes'] and 'WEIGHTS_0' in q['attributes']
# Frame 16 diagnoses low-amplitude animation on the actual skinned mesh.
scene=bpy.context.scene; scene.frame_set(16); scene.render.engine='CYCLES'; scene.cycles.samples=24; scene.cycles.use_denoising=False
scene.render.resolution_x=768; scene.render.resolution_y=1024; scene.render.resolution_percentage=100
scene.world.color=(.12,.12,.12); scene.view_settings.view_transform='AgX'
for loc,power in [((-2,-3,3),400),((2,-2,2),220),((0,2,3),350)]:
 bpy.ops.object.light_add(type='AREA',location=loc); l=bpy.context.object; l.data.energy=power; l.data.size=3; l.rotation_euler=(Vector((0,0,.9))-l.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(0,-4,.86)); cam=bpy.context.object; cam.rotation_euler=(Vector((0,0,.86))-cam.location).to_track_quat('-Z','Y').to_euler(); cam.data.type='ORTHO'; cam.data.ortho_scale=2.05; scene.camera=cam
scene.render.filepath=str(root/'DTE_Prototype_Preview.png'); bpy.ops.render.render(write_still=True)
# Independent importer round trip of the delivered FBX.
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root/'DTE_Avatar_Prototype.fbx'),use_anim=True)
meshes2=[o for o in bpy.context.scene.objects if o.type=='MESH']; arms2=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
assert len(meshes2)==1 and len(arms2)==1
bpy.context.scene.frame_set(1); bpy.context.view_layer.update()
verts=[o.matrix_world@v.co for o in meshes2 for v in o.data.vertices]
height=max(v.z for v in verts)-min(v.z for v in verts)
assert abs(height-1.72)<.02, 'Unexpected FBX scale: {}'.format(height)
assert len(arms2[0].data.bones)==27
assert any(o.modifiers and any(m.type=='ARMATURE' for m in o.modifiers) for o in meshes2)
report={'blender_roundtrip_pass':True,'fbx_height_m':height,'fbx_mesh_count':len(meshes2),'fbx_armature_count':len(arms2),'fbx_bones':len(arms2[0].data.bones),'glb_skin_count':len(j['skins']),'glb_joint_count':len(j['skins'][0]['joints']),'glb_animations':[x.get('name') for x in j['animations']],'vertices_without_weights':len(zero),'maximum_weight_sum_error':max(errors),'native_unreal_import_tested':False,'production_deformation_quality_certified':False}
(root/'validation.json').write_text(json.dumps(report,indent=2)+'\n'); print('VALIDATION',json.dumps(report))
