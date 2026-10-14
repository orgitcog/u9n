"""Prototype-only humanoid skinning and UE export for the supplied DTE GLB.
Run: blender -b --python build_prototype.py -- --input avatar.glb --out Assets
The heuristic skinning is not production retopology or a MetaHuman rig.
"""
import bpy, math, json, sys, argparse
from pathlib import Path
from mathutils import Vector, Matrix

p=argparse.ArgumentParser()
p.add_argument('--input', required=True); p.add_argument('--out', required=True)
p.add_argument('--height',type=float,default=1.72)
p.add_argument('--yaw',type=float,default=-90.0,help='Source +X forward to Blender -Y forward')
a=p.parse_args(sys.argv[sys.argv.index('--')+1:])
out=Path(a.out).resolve(); out.mkdir(parents=True,exist_ok=True)
tex=out/'Textures'; tex.mkdir(exist_ok=True)
H=a.height
if not math.isfinite(H) or not .5 <= H <= 3.0: raise ValueError('Expected character height in [0.5, 3] meters')
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(Path(a.input).resolve()))
mesh_objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
if not mesh_objects: raise ValueError('No meshes in input')
for o in mesh_objects: o.matrix_world=Matrix.Rotation(math.radians(a.yaw),4,'Z')@o.matrix_world
bpy.context.view_layer.update()
coords=[o.matrix_world@v.co for o in mesh_objects for v in o.data.vertices]
lo=Vector([min(v[i] for v in coords) for i in range(3)]); hi=Vector([max(v[i] for v in coords) for i in range(3)])
scale=H/(hi.z-lo.z); center=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
for o in mesh_objects:
 transform=o.matrix_world.copy()
 for v in o.data.vertices: v.co=(transform@v.co-center)*scale
 o.matrix_world=Matrix.Identity(4)
 o.name='DTE_Avatar_Prototype'
 o.data.update()
bpy.context.scene.unit_settings.system='METRIC'; bpy.context.scene.unit_settings.scale_length=1.0
# Save external PBR images. ORM channels are R=occlusion/G=roughness/B=metallic.
texture_records=[]
for img in list(bpy.data.images):
 if img.type!='IMAGE' or img.size[0]==0: continue
 key='BaseColor' if img.name.startswith('Color_') else 'NormalGL' if img.name.startswith('NormalGL_') else 'ORM' if img.name.startswith('ORM_') else img.name.split('.')[0]
 img.filepath_raw=str(tex/(key+'.png')); img.file_format='PNG'; img.save()
 texture_records.append({'filename':'Textures/'+key+'.png','width':img.size[0],'height':img.size[1]})
# Body center is inferred from central torso vertices, not from wing-heavy bbox.
trunk=[v.co.y for o in mesh_objects for v in o.data.vertices if abs(v.co.x)<.065*H and .56*H<v.co.z<.74*H]
trunk.sort(); Y=trunk[len(trunk)//2] if trunk else 0.0
spec=[]
def bone(name,head,tail,parent=None): spec.append((name,Vector(head)*H+Vector((0,Y,0)),Vector(tail)*H+Vector((0,Y,0)),parent))
bone('root',(0,0,0),(0,0,.06))
bone('pelvis',(0,0,.53),(0,0,.58),'root')
bone('spine_01',(0,0,.58),(0,0,.65),'pelvis')
bone('spine_02',(0,0,.65),(0,0,.72),'spine_01')
bone('spine_03',(0,0,.72),(0,0,.81),'spine_02')
bone('neck_01',(0,0,.81),(0,0,.87),'spine_03')
bone('head',(0,0,.87),(0,0,.98),'neck_01')
for side,sign in [('l',1),('r',-1)]:
 def q(x,y,z): return (sign*x,y,z)
 bone('clavicle_'+side,q(.015,0,.79),q(.105,0,.79),'spine_03')
 bone('upperarm_'+side,q(.105,0,.79),q(.187,-.006,.645),'clavicle_'+side)
 bone('lowerarm_'+side,q(.187,-.006,.645),q(.254,-.01,.548),'upperarm_'+side)
 bone('hand_'+side,q(.254,-.01,.548),q(.290,-.012,.508),'lowerarm_'+side)
 bone('thigh_'+side,q(.065,0,.525),q(.083,-.009,.325),'pelvis')
 bone('calf_'+side,q(.083,-.009,.325),q(.095,0,.092),'thigh_'+side)
 bone('foot_'+side,q(.095,0,.092),q(.095,-.069,.035),'calf_'+side)
 bone('ball_'+side,q(.095,-.069,.035),q(.095,-.099,.035),'foot_'+side)
 bone('wing_root_'+side,q(.10,.065,.785),q(.22,.070,.80),'spine_03')
 bone('wing_tip_'+side,q(.22,.070,.80),q(.265,.065,.28),'wing_root_'+side)
armdata=bpy.data.armatures.new('DTE_Prototype_Skeleton')
arm=bpy.data.objects.new('DTE_Rig',armdata); bpy.context.collection.objects.link(arm)
bpy.context.view_layer.objects.active=arm; arm.select_set(True); bpy.ops.object.mode_set(mode='EDIT')
for name,head,tail,parent in spec:
 b=armdata.edit_bones.new(name); b.head=head; b.tail=tail
 if parent: b.parent=armdata.edit_bones[parent]
 b.use_deform=name!='root'
bpy.ops.object.mode_set(mode='OBJECT'); arm.show_in_front=True
segments={n:(h,t) for n,h,t,_ in spec}
def distance(v,seg):
 h,t=seg; d=t-h; u=max(0,min(1,(v-h).dot(d)/d.length_squared)); return (v-(h+u*d)).length
wing_vertices=0; max_error=0.0; max_influences=0
for o in mesh_objects:
 groups={n:o.vertex_groups.new(name=n) for n,_,_,_ in spec if n!='root'}
 for v in o.data.vertices:
  x,y,z=v.co; side='l' if x>=0 else 'r'; ax=abs(x)/H; zz=z/H; yy=(y-Y)/H
  is_wing=(yy>.035 and ax>.125 and .22<zz<.92)
  if is_wing:
   # These rigid mechanical appendages must not inherit hand or leg weights.
   names=['wing_root_'+side,'wing_tip_'+side]; wing_vertices+=1
  elif zz>.855: names=['head','neck_01']
  elif zz>.79 and ax<.10: names=['neck_01','head','spine_03']
  elif ax>.125 and .48<zz<.825:
   names=['clavicle_'+side,'upperarm_'+side,'lowerarm_'+side,'hand_'+side]
  elif zz<.49:
   names=['thigh_'+side,'calf_'+side,'foot_'+side,'ball_'+side]
  elif zz<.565: names=['pelvis','spine_01']
  else: names=['pelvis','spine_01','spine_02','spine_03','neck_01']
  ranked=sorted((distance(v.co,segments[n]),n) for n in names)[:3]
  weights=[1.0/(d+.009*H)**4 for d,n in ranked]; total=sum(weights)
  kept=[(n,w/total) for (d,n),w in zip(ranked,weights) if w/total>.01]
  total2=sum(w for n,w in kept)
  for n,w in kept: groups[n].add([v.index],w/total2,'REPLACE')
  max_error=max(max_error,abs(sum(w/total2 for n,w in kept)-1)); max_influences=max(max_influences,len(kept))
 mod=o.modifiers.new('DTE_Prototype_Skin','ARMATURE'); mod.object=arm; mod.use_deform_preserve_volume=False
 o.parent=arm
# A low-amplitude authored diagnostic idle, not extracted from the source videos.
scene=bpy.context.scene; scene.render.fps=30; scene.frame_start=1; scene.frame_end=61
arm.animation_data_create(); act=bpy.data.actions.new('DTE_Idle_WingPulse_Diagnostic'); arm.animation_data.action=act
for pb in arm.pose.bones: pb.rotation_mode='XYZ'
for frame in range(1,62,5):
 phase=2*math.pi*(frame-1)/60
 for name in ['spine_02','head','wing_root_l','wing_root_r']:
  pb=arm.pose.bones[name]; pb.rotation_euler=(0,0,0)
  if name=='spine_02': pb.rotation_euler.x=.01*math.sin(phase)
  elif name=='head': pb.rotation_euler.y=.02*math.sin(phase)
  else: pb.rotation_euler.z=(1 if name.endswith('_l') else -1)*.06*math.sin(phase)
  pb.keyframe_insert('rotation_euler',frame=frame,group=name)
track=arm.animation_data.nla_tracks.new(); track.name='DTE_Idle_WingPulse_Diagnostic'; strip=track.strips.new(act.name,1,act)
arm.animation_data.action=None
scene.frame_set(1)
bpy.ops.object.select_all(action='DESELECT'); arm.select_set(True)
for o in mesh_objects: o.select_set(True)
bpy.context.view_layer.objects.active=arm
bpy.ops.export_scene.fbx(filepath=str(out/'DTE_Avatar_Prototype.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',global_scale=1.0,use_mesh_modifiers=True,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=True,bake_anim_simplify_factor=0.0,path_mode='RELATIVE')
bpy.ops.export_scene.gltf(filepath=str(out/'DTE_Avatar_Prototype.glb'),export_format='GLB',use_selection=True,export_skins=True,export_animations=True,export_nla_strips=True,export_force_sampling=True)
bpy.ops.file.pack_all(); bpy.ops.wm.save_as_mainfile(filepath=str(out/'DTE_Avatar_Prototype.blend'))
report={'status':'experimental_skinning_prototype','height_m':H,'vertices':sum(len(o.data.vertices) for o in mesh_objects),'triangles':sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in mesh_objects),'bones':len(spec),'wing_classified_vertices':wing_vertices,'max_influences':max_influences,'max_weight_sum_error':max_error,'textures':texture_records,'animations':['DTE_Idle_WingPulse_Diagnostic'],'has_facial_morph_targets':False,'has_finger_bones':False,'compatible_with_metahuman_dna':False,'native_unreal_import_tested':False,'weight_method':'position-based anatomical-region + inverse segment distance; artist correction required','blender_forward':'-Y','blender_up':'+Z','fbx_export_forward':'-Y','fbx_export_up':'+Z','unit_system':'meters (FBX unit metadata exported)','skeleton':[{'name':n,'parent':par,'head_m':list(h),'tail_m':list(t)} for n,h,t,par in spec]}
(out/'prototype-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
assert max_error<1e-6 and max_influences<=3
print('PROTOTYPE BUILT', json.dumps({k:v for k,v in report.items() if k not in ['skeleton','textures']}))
