"""Original shonen-anime monster meshes: Blender 4.3+, cm, +X front, +Z up.
Run blender -b --python Scripts/generate_monster_meshes.py -- --preview.
FBX parts use identity node transforms; not skeletons or external downloaded assets.
"""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Monsters'
PARTS=('Body','Head','Hair','LeftArm','RightArm','LeftLeg','RightLeg','Weapon')
SLOTS=('Cloth','Metal','Accent','Skin','Glow','Ink','White')
COLORS=((.15,.045,.04,1),(.23,.28,.34,1),(.65,.12,.035,1),(.5,.36,.27,1),(1,.5,.04,1),(.015,.025,.04,1),(.7,.73,.72,1))
class Mesh:
    def __init__(self): self.v=[]; self.f=[]; self.m=[]; self.s=[]
    def face(self, indices, material, smooth=True): self.f.append(indices); self.m.append(material); self.s.append(smooth)
    def rings(self, profiles, material, count=32, phase=0):
        """z, x-radius, y-radius, x-offset, y-offset; elliptical sculpted cross sections."""
        start=len(self.v)
        for z,rx,ry,x,y in profiles:
            for i in range(count):
                a=2*math.pi*i/count+phase
                self.v.append((x+rx*math.cos(a),y+ry*math.sin(a),z))
        self.face(tuple(start+i for i in reversed(range(count))),material)
        for j in range(len(profiles)-1):
            for i in range(count):
                a=start+j*count+i;b=start+j*count+(i+1)%count
                self.face((a,b,b+count,a+count),material)
        end=start+(len(profiles)-1)*count
        self.face(tuple(end+i for i in range(count)),material)
    def prism(self, points, thickness, material):
        start=len(self.v); n=len(points)
        self.v.extend(points);self.v.extend([(x-thickness,y,z) for x,y,z in points])
        self.face(tuple(start+i for i in range(n)),material,False)
        self.face(tuple(start+n+i for i in reversed(range(n))),material,False)
        for i in range(n): self.face((start+i,start+(i+1)%n,start+n+(i+1)%n,start+n+i),material,False)
    def ellipsoid(self, center, radii, material, count=16, rows=8):
        profiles=[]
        for i in range(rows+1):
            a=math.pi*i/rows
            profiles.append((center[2]+radii[2]*math.cos(a), max(.01,radii[0]*math.sin(a)),max(.01,radii[1]*math.sin(a)),center[0],center[1]))
        self.rings(list(reversed(profiles)),material,count)
    def export(self,name):
        mesh=bpy.data.meshes.new(name);mesh.from_pydata(self.v,[],self.f);mesh.update()
        obj=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(obj)
        for mat in MATERIALS: mesh.materials.append(mat)
        for poly,mat,smooth in zip(mesh.polygons,self.m,self.s): poly.material_index=mat;poly.use_smooth=smooth
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
        # Explicit triangulation and UVs support predictable normal and material import.
        triangulate=obj.modifiers.new('Triangulated authored topology','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=triangulate.name)
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False) if hasattr(bpy.ops.mesh,'normals_make_consistent') else None
        bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.025);bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='Y',axis_up='Z',use_space_transform=False,bake_space_transform=False,apply_unit_scale=True,global_scale=1.0,add_leaf_bones=False,bake_anim=False,path_mode='STRIP')
        data={'name':name,'vertices':len(mesh.vertices),'triangles':len(mesh.polygons),'bounds_cm':[[min(v.co[i] for v in mesh.vertices) for i in range(3)],[max(v.co[i] for v in mesh.vertices) for i in range(3)]]}
        bpy.data.objects.remove(obj,do_unlink=True)
        return data

def tube(m,a,b,radius,material,count=12):
    direction=Vector(b)-Vector(a);axis=direction.normalized()
    cross=axis.cross(Vector((0,1,0)))
    if cross.length<.01:cross=axis.cross(Vector((1,0,0)))
    u=cross.normalized();v=axis.cross(u);start=len(m.v)
    for center,r in ((Vector(a),radius),(Vector(b),radius*.88)):
        for j in range(count):
            p=center+(u*math.cos(j*math.tau/count)+v*math.sin(j*math.tau/count))*r;m.v.append(tuple(p))
    m.face(tuple(start+j for j in reversed(range(count))),material,False)
    for j in range(count):m.face((start+j,start+(j+1)%count,start+(j+1)%count+count,start+j+count),material,True)
    m.face(tuple(start+count+j for j in range(count)),material,False)

def spike(m,a,b,radius,material):
    start=len(m.v);axis=(Vector(b)-Vector(a)).normalized();u=axis.cross(Vector((1,0,0)))
    if u.length<.01:u=axis.cross(Vector((0,1,0)))
    u.normalize();v=axis.cross(u)
    for j in range(8):m.v.append(tuple(Vector(a)+(u*math.cos(j*math.tau/8)+v*math.sin(j*math.tau/8))*radius))
    m.v.append(b)
    m.face(tuple(start+j for j in reversed(range(8))),material,False)
    for j in range(8):m.face((start+j,start+(j+1)%8,start+8),material,False)

def plate(m,x,y,z,w,h,material=1):
    # Faceted, chamfered armor plate, no paper-thin coplanar overlay.
    m.prism([(x,y-w*.36,z+h*.5),(x+1.5,y+w*.36,z+h*.5),(x+3,y+w*.5,z+h*.32),(x+4,y+w*.43,z-h*.4),(x+2,y,z-h*.55),(x+4,y-w*.43,z-h*.4),(x+3,y-w*.5,z+h*.32)],2.2,material)

def body(role):
    m=Mesh();heavy=role in (1,3);width=23 if heavy else (17 if role==2 else 19)
    m.rings([(-31,12,15,0,0),(-17,13,16,0,0),(-4,12,15,0,0),(18,17 if heavy else 14,width,0,0),(32,15,width,0,0),(39,10,17,0,0),(43,7,7,0,0)],0,24)
    m.rings([(39,6,6,0,0),(48,5.5,5.5,0,0)],5)
    m.rings([(-25,14,17,0,0),(-18,14.5,17.5,0,0)],1)
    plate(m,15,0,-22,11,9,2)
    # Reference-specific torso: fine overlapping red scales, broad amber plates,
    # long indigo coat, or the boss's caged glowing furnace core.
    if role==0:
        for row in range(4):
            for col in range(3):plate(m,14.5-abs(col-1)*2,(col-1)*10,29-row*10,11,13,2 if row%2==0 else 1)
    elif role==1:
        for row in range(4):
            z=29-row*12
            m.prism([(17,-20,z+6),(17,20,z+6),(20,18,z-6),(20,-18,z-6)],3,2)
            for y in (-12,0,12):m.ellipsoid((21,y,z),(1,1.3,1.3),6,8,4)
    elif role==2:
        for side in (-1,1):
            m.prism([(13,side*5,35),(16,side*17,22),(14,side*14,-20),(12,side*4,-19)],2,2)
            m.prism([(12,side*4,-24),(8,side*18,-23),(8,side*25,-70),(13,side*2,-72)],2,2)
        for z in (-8,5,18):m.ellipsoid((17,0,z),(1,1.5,1.5),6,8,4)
    else:
        tube(m,(16,0,14),(22,0,14),19,1,24)
        tube(m,(22,0,14),(24,0,14),15,4,24)
        for y in (-10,-5,0,5,10):tube(m,(25,y,3),(25,y,25),1.1,1,8)
        for j in range(16):
            ang=j*math.tau/16;m.ellipsoid((24,17*math.cos(ang),14+17*math.sin(ang)),(1.2,1.5,1.5),2,8,4)
    # Layered thigh apron gives a distinctive Japanese armored silhouette.
    for side in (-1,1):
        for j in range(3):
            m.prism([(10,side*4,-27-j*9),(8,side*16,-26-j*9),(8,side*(20+j*2),-37-j*9),(13,side*6,-38-j*9)],2,2 if j==0 else 1)
        m.prism([(-9,side*4,-24),(-10,side*19,-24),(-14,side*26,-65),(-15,side*4,-69)],2,0)
    if role==2:
        # Energy fuel backpack with an inset luminous viewing slit.
        m.rings([(-12,10,11,-23,0),(30,10,11,-23,0)],1,16)
        tube(m,(-23,-11,5),(-23,-11,24),3,4)
        for z in (-10,29):m.rings([(z,12,13,-23,0),(z+4,12,13,-23,0)],2,16)
        tube(m,(-25,0,24),(-10,-22,28),2.3,1)
    if role==3:
        m.rings([(-6,15,20,-24,0),(35,15,20,-24,0)],1,16)
        plate(m,-38,0,14,28,28,2)
        for side in (-1,1):
            m.rings([(18,5,5,-25,side*13),(58,6.5,6.5,-25,side*13),(62,8,8,-25,side*13),(66,8,8,-25,side*13)],1,12)
            m.rings([(61,8.2,8.2,-25,side*13),(65,8.2,8.2,-25,side*13)],2,12)
            m.rings([(66,6,6,-25,side*13),(67,6,6,-25,side*13)],4,12)
        for side in (-1,1):
            for yoff in (-2,2):tube(m,(-18,side*13+yoff,23),(-18,side*13+yoff,58),1.2,4)

        for side in (-1,1):spike(m,(3,side*20,38),(-2,side*31,47),5,6)
    return m

def head(role):
    m=Mesh()
    m.rings([(46,4,6,0,0),(49,7,9,1,0),(56,11,12,0,0),(66,12,13,-1,0),(75,10,12,-2,0),(79,4,6,-2,0)],5,24)
    # Oni/puppet face with recessed dark eye sockets and bright narrow eyes.
    for side in (-1,1):
        plate(m,10,side*6,57,10,11,6 if role in (0,3) else 1)
        m.prism([(12,side*1.5,66),(12,side*11,68),(12,side*10,63),(12,side*2,63)],.8,5)
        m.prism([(13,side*2.5,65.5),(13,side*9.5,66.4),(13,side*8.5,64),(13,side*3,64)],.3,4)
        if role in (0,3):spike(m,(12,side*5,53),(15,side*5,60),1.8,6)
    if role==2:
        # Long ivory plague-bird beak: main silhouette of the ranged puppet.
        m.prism([(12,-5,64),(12,5,64),(30,0,48)],4,6)
        m.prism([(14,-.8,62),(14,.8,62),(30,0,48)],.6,2)
    else:m.prism([(12,-2,63),(12,2,63),(17,0,58)],2,1)
    plate(m,12,0,51,12,8,1)
    # Vent slits instead of a floating mouth.
    for y in (-4,0,4):tube(m,(15,y,49),(15,y,53),.65,5,6)
    return m

def helmet(role):
    m=Mesh()
    if role==2:
        m.rings([(61,12,14,-5,0),(71,14,16,-4,0),(80,11,14,-5,0),(91,2,3,-10,0),(93,.2,.3,-11,0)],2,20)
        # Open front at face height and sweeping cheek guards.
        for side in (-1,1):m.prism([(4,side*12,77),(-5,side*16,76),(-7,side*17,48),(2,side*12,53)],2,1)
    else:
        m.rings([(69,12,15,-4,0),(77,13,16,-3,0),(83,9,12,-4,0),(85,2,4,-4,0)],1,20)
        tube(m,(9,-13,73),(9,13,73),2,2)
        for side in (-1,1):
            m.prism([(-4,side*14,76),(-10,side*16,73),(-11,side*17,52),(-2,side*14,55)],2,2)
            if role in (0,3):
                tube(m,(1,side*10,80),(3,side*15,91 if role==3 else 87),3.5,6)
                spike(m,(3,side*15,91 if role==3 else 87),(9,side*20,105 if role==3 else 97),3.2,6)
        if role==1:spike(m,(-4,0,83),(1,0,97),5,2)
        if role==3:
            m.prism([(8,-4,82),(10,0,93),(8,4,82),(13,0,77)],2,2)
    return m

def arm(role,side):
    m=Mesh();heavy=role in (1,3)
    m.rings([(-43,5,6,0,0),(-31,7,8,0,0),(-21,7,8,0,0),(-9,8,10,0,0),(4,10 if heavy else 8,11 if heavy else 9,0,0)],1,20)
    if role==2:
        m.prism([(-10,-12,5),(9,-12,5),(12,0,-8),(9,12,5),(-10,12,5)],3,2)
    else:
        for layer in range(3):
            z=7-layer*8;w=(15 if heavy else 12)+layer*1.5
            m.prism([(9,-w,z),(9,w,z),(12,w+2,z-7),(13,0,z-11),(12,-w-2,z-7)],3,2)
            for y in (-w*.65,w*.65):m.ellipsoid((13,y,z-3),(1,1.2,1.2),6,8,4)
        if heavy:spike(m,(-1,side*9,5),(-5,side*19,17),4,6)
    plate(m,8,0,-29,13,20,2)
    m.rings([(-40,7,8,0,0),(-36,7.5,8.5,0,0)],6,16)
    m.ellipsoid((1,0,-46),(6,6,7),1,12,8)
    for y in (-3,0,3):m.ellipsoid((5,y,-49),(2.7,1.6,3.5),1,10,5)
    return m

def shield():
    m=Mesh()
    outline=[(4,-25,42),(4,25,42),(6,32,28),(6,26,-30),(6,0,-43),(6,-26,-30),(6,-32,28)]
    m.prism(outline,12,1)
    m.prism([(8,-21,35),(8,21,35),(10,25,24),(10,20,-25),(10,0,-36),(10,-20,-25),(10,-25,24)],2,2)
    # Circular ward seal, two rings and angular inner motif, no opaque white T.
    for radius in (19,24):
        for j in range(32):
            a=j*math.tau/32;b=(j+1)*math.tau/32
            tube(m,(13,radius*math.cos(a),radius*math.sin(a)),(13,radius*math.cos(b),radius*math.sin(b)),.65,4,6)
    for a,b in [((14,-8,10),(14,8,10)),((14,8,10),(14,8,-10)),((14,8,-10),(14,-8,-10)),((14,-8,-10),(14,-8,10)),((14,-12,0),(14,12,0)),((14,0,-15),(14,0,15))]:tube(m,a,b,1,4,6)
    for y in (-21,21):
        for z in (-20,22):m.ellipsoid((10,y,z),(1.8,2,2),6,8,4)
    return m

def leg(role):
    m=Mesh();heavy=role in (1,3)
    m.rings([(-45,6,7,0,0),(-35,7,8,0,0),(-23,8,9,0,0),(-10,9 if heavy else 8,10 if heavy else 9,0,0),(5,9 if heavy else 8,10 if heavy else 9,0,0)],0,20)
    plate(m,9,0,-22,15,15,2)
    m.rings([(-42,7,8,0,0),(-34,8,9,0,0),(-29,7,8,0,0)],1,20)
    plate(m,8,0,-35,13,18,1)
    m.rings([(-49,13,8,6,0),(-46,15,9,7,0),(-41,12,8,5,0),(-38,7,7,0,0)],1,20)
    m.rings([(-49,13.5,8.5,6,0),(-47,13.5,8.5,6,0)],6,20)
    return m

def weapon(role):
    m=Mesh()
    if role==0:
        tube(m,(-20,0,0),(8,0,0),2.7,5)
        tube(m,(4,-9,0),(4,9,0),1.8,2)
        m.prism([(8,-1,0),(60,-1,2),(88,-1,6),(98,-1,11),(84,-1,13),(60,-1,8),(8,-1,5)],2,1)
        m.prism([(8,-1.4,0),(60,-1.4,2),(88,-1.4,6),(98,-1.4,11),(86,-1.4,9),(60,-1.4,4),(8,-1.4,2)],.3,6)
    elif role==1:
        tube(m,(-20,0,0),(8,0,0),3,5)
        tube(m,(4,-10,0),(4,10,0),2,2)
        # Broad curved single-edge cleaver, bevel ridge and silver cutting edge.
        m.prism([(8,-2,-8),(70,-2,-8),(93,-2,0),(83,-2,14),(33,-2,14),(8,-2,5)],4,1)
        m.prism([(10,-2.6,-8),(70,-2.6,-8),(93,-2.6,0),(70,-2.6,-3),(10,-2.6,-3)],.3,6)
        for x in (23,38,53):tube(m,(x,-3,8),(x,-3,11),1.1,2,6)
    elif role==2:
        tube(m,(-14,0,0),(114,0,0),4.5,1,16)
        for x in (4,36,85,108):tube(m,(x,0,0),(x+6,0,0),7,2,16)
        tube(m,(116,0,0),(124,0,0),6,1,16)
        tube(m,(124,0,0),(125,0,0),4,4,16)
        m.prism([(-21,-7,6),(12,-7,6),(19,-7,-6),(-17,-7,-20),(-26,-7,-17)],14,0)
        tube(m,(33,0,6),(33,0,16),2,1)
        tube(m,(29,0,17),(44,0,17),3,4)
    else:
        tube(m,(-22,0,0),(106,0,0),5,1,16)
        for x in (-14,12,38):tube(m,(x,0,0),(x+4,0,0),5.8,2,16)
        # Chamfered rectangular forge hammer, with caged glowing front panel.
        m.prism([(119,-26,-25),(119,26,-25),(123,32,-19),(123,32,19),(119,26,25),(119,-26,25),(123,-32,19),(123,-32,-19)],39,1)
        m.prism([(126,-23,-19),(126,23,-19),(126,23,19),(126,-23,19)],3,2)
        m.prism([(128,-18,-15),(128,18,-15),(128,18,15),(128,-18,15)],2,4)
        for y in (-12,-6,0,6,12):tube(m,(130,y,-17),(130,y,17),1.3,1,8)
        for side in (-1,1):
            spike(m,(101,side*25,23),(101,side*30,39),5,6)
            spike(m,(101,side*25,-23),(101,side*30,-36),4,6)
    return m

bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
MATERIALS=[]
for name,color in zip(SLOTS,COLORS):
    mat=bpy.data.materials.new(name);mat.diffuse_color=color;MATERIALS.append(mat)
OUT.mkdir(parents=True,exist_ok=True)
records=[]
for role in range(4):
    meshes=(body(role),head(role),helmet(role),arm(role,-1),arm(role,1),leg(role),leg(role),weapon(role))
    for part,mesh in zip(PARTS,meshes):records.append(mesh.export(f'SM_AlphaMonster{role}_{part}'))
records.append(shield().export('SM_AlphaMonster1_Shield'))
(OUT/'manifest.json').write_text(json.dumps({'revision':1,'generator':'Scripts/generate_monster_meshes.py','coordinates':'centimeters, +X front, +Z up, identity FBX nodes, arm/leg/weapon local pivots','material_slots':SLOTS,'meshes':records},indent=2)+'\n')
print('Generated',len(records),'monster meshes;',sum(r['triangles'] for r in records),'triangles')
if '--preview' in sys.argv:
    for role in range(4):
        for part in PARTS+(('Shield',) if role==1 else ()):
            name=f'SM_AlphaMonster{role}_{part}'
            bpy.ops.import_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_custom_normals=True,use_manual_orientation=True,axis_forward='Y',axis_up='Z',global_scale=1.)
            obj=bpy.context.selected_objects[0]
            assert max(abs(v-1) for v in obj.scale)<.001 and max(abs(v) for v in obj.rotation_euler)<.001,(name,obj.scale[:],obj.rotation_euler[:])
            assert max(abs(v) for v in obj.location)<.001,(name,obj.location[:])
            expected=next(r for r in records if r['name']==name)
            observed=[[min(v.co[i] for v in obj.data.vertices) for i in range(3)],[max(v.co[i] for v in obj.data.vertices) for i in range(3)]]
            assert max(abs(a-b) for aa,bb in zip(expected['bounds_cm'],observed) for a,b in zip(aa,bb))<.001,(name,observed)
            pivot={'LeftArm':(0,-24,34),'RightArm':(0,24,34),'LeftLeg':(0,-12,-29),'RightLeg':(0,12,-29),'Weapon':(32,-30,20),'Shield':(34,-20,0)}.get(part,(0,0,0))
            scale=1.35 if role==3 else 1
            obj.scale=(scale,scale,scale)
            # Preview-only relaxed weapon/ward poses expose chest and core; FBX pivots stay unchanged.
            if role==3 and part=='Weapon':pivot=(20,62,45);obj.rotation_euler[1]=math.radians(55)
            if part=='Shield':pivot=(30,-65,0)
            obj.location=(pivot[0]*scale,pivot[1]*scale+role*155,pivot[2]*scale)
            for slot in obj.material_slots:
                if not slot.material:continue
                base=slot.material.name.split('.')[0];mat=slot.material.copy();slot.material=mat
                accent=((.65,.06,.025,1),(.7,.34,.06,1),(.03,.25,.65,1),(.5,.075,.025,1))[role]
                if base=='Accent':mat.diffuse_color=accent
                if base=='Cloth' and role==2:mat.diffuse_color=(.035,.045,.1,1)
                if base=='Glow':mat.diffuse_color=(.1,.7,1,1) if role==2 else (1,.45,.02,1)
    scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO'
    scene.display.shading.color_type='MATERIAL';scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH'
    scene.display.shading.background_type='WORLD';scene.world.color=(.7,.7,.7)
    bpy.ops.object.camera_add(location=(650,5,150));camera=bpy.context.object;camera.rotation_euler=(Vector((10,265,5))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=850;scene.camera=camera
    scene.render.resolution_x=1900;scene.render.resolution_y=950;scene.render.resolution_percentage=100
    target=OUT/'References';target.mkdir(parents=True,exist_ok=True);scene.render.filepath=str(target/'ModeledMonstersPreview.png');bpy.ops.render.render(write_still=True)
    (target/'model_preview.json').write_text(json.dumps({'generator':'Scripts/generate_monster_meshes.py -- --preview','note':'Offline Blender Workbench render of reimported exported FBX meshes, not an Unreal screenshot. Identity node transforms and local mesh bounds verified within 0.001cm. UE import and rendering remain unverified.'},indent=2)+'\n')
