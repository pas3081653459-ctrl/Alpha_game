"""Original modular anime meshes. Run: blender -b --python Scripts/generate_character_meshes.py.
Coordinates are authored in centimeters, +X facing, +Z up; no downloaded models.
FBX parts retain limb pivot origins for the existing cosmetic pose animation.
"""
import bpy
import json
import math
import sys
from mathutils import Vector
from mathutils.geometry import tessellate_polygon
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt/Characters'
SLOTS = ('Cloth', 'Metal', 'Accent', 'Skin', 'Glow', 'Ink', 'White')
COLORS = ((.3,.03,.05,1),(.09,.12,.18,1),(.7,.06,.08,1),(.85,.65,.5,1),(1,.6,.1,1),(.012,.018,.035,1),(.88,.91,.95,1))

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

def body(hero):
    m=Mesh()
    profiles=[(-30,12,15,0,0),(-18,12 if hero==1 else 14,14 if hero==1 else 16,0,0),(-2,10 if hero==1 else 12,12 if hero==1 else 14,0,0),(19,14 if hero==1 else 16,18 if hero==1 else 20,0,0),(34,10 if hero==1 else 12,18 if hero==1 else 21,0,0),(40,9,15,0,0),(42,7,8,0,0)]
    m.rings(profiles,6 if hero in (1,2) else 0)
    def patch(points,material,offset=1.3):
        # Conform tessellated garment appliques to curved torso; flat chord polygons sink inside it.
        cache={}
        def vertex(y,z):
            key=(round(y,5),round(z,5))
            if key in cache:return cache[key]
            x=10
            for lower,upper in zip(profiles,profiles[1:]):
                if lower[0]<=z<=upper[0]:
                    t=(z-lower[0])/(upper[0]-lower[0]);rx=lower[1]+(upper[1]-lower[1])*t;ry=lower[2]+(upper[2]-lower[2])*t
                    x=rx*math.sqrt(max(.02,1-(y/ry)**2))+offset;break
            cache[key]=len(m.v);m.v.append((x,y,z));return cache[key]
        for triangle in tessellate_polygon([[Vector(point) for point in points]]):
            a,b,c=[points[value] if isinstance(value,int) else value for value in triangle]
            n=12
            def p(i,j):
                return vertex(a[1]+(b[1]-a[1])*i/n+(c[1]-a[1])*j/n,a[2]+(b[2]-a[2])*i/n+(c[2]-a[2])*j/n)
            def tri(ids):
                va,vb,vc=(m.v[v] for v in ids)
                nx=(vb[1]-va[1])*(vc[2]-va[2])-(vb[2]-va[2])*(vc[1]-va[1])
                m.face(ids if nx>0 else tuple(reversed(ids)),material)
            for i in range(n):
                for j in range(n-i):
                    tri((p(i,j),p(i+1,j),p(i,j+1)))
                    if i+j<n-1:tri((p(i+1,j),p(i+1,j+1),p(i,j+1)))
    m.rings([(38,6,6,0,0),(48,5,5,0,0)],3)
    # Shaped opened jacket lapels, undershirt, tailored waist and layered hem.
    patch([(15,-7,33),(18,0,29),(15,7,33),(15,7,-12),(14,-7,-12)],6 if hero != 0 else 5,.7)
    for s in (-1,1):
        patch([(13,s*8,38),(17,s*15,26),(17,s*10,3),(15,s*8,15)],2)
        patch([(13,s*8,38),(17,s*8.8,36),(17,s*10.8,3),(15,s*10,3)],6,1.6)
        patch([(12,s*16,30),(15,s*17,24),(12,s*14,-12),(10,s*10,-18)],0 if hero==0 else 6)
    m.rings([(-24,13,16,0,0),(-17,14,17,0,0)],5)
    m.prism([(15,-4,-17),(16,4,-17),(16,4,-24),(15,-4,-24)],2,2)
    if hero in (0,2):
        length=48 if hero==0 else 53
        for s in (-1,1):
            m.prism([(-10,s*3,-20),(-9,s*17,-20),(-15,s*25,-20-length),(-19,s*6,-22-length)],3,0 if hero==0 else 6)
            m.prism([(-7,s*16,-21),(-6,s*18,-21),(-12,s*26,-20-length),(-13,s*24,-20-length)],1,2)
            m.prism([(12,s*7,-23),(5,s*17,-23),(8,s*25,-20-length),(14,s*10,-17-length)],1.2,0 if hero==0 else 6)
            m.prism([(14,s*10,-17-length),(8,s*25,-20-length),(9,s*25,-17-length),(15,s*10,-14-length)],.5,6 if hero==0 else 2)
    if hero==1:
        m.rings([(34,12,23,0,0),(39,11,21,0,0),(41,9,15,0,0)],2)
        m.prism([(-12,-9,36),(-24,-12,34),(-33,-8,20),(-26,-2,17),(-17,-2,32)],2,2)
    if hero==2:
        patch([(17,-10,34),(19,0,39),(17,10,34),(18,0,26)],2,1.8)
    return m

def head(hero):
    m=Mesh()
    m.rings([(47,3,4,3,0),(49,6,7,2,0),(54,10,11,0,0),(62,12,13,-1,0),(70,11.5,12,-2,0),(77,9,10,-2,0),(80,4,6,-2,0)],3,24)
    # Anime eye sockets, whites, iris, pupil, brows and pointed nose are actual meshes.
    for s in (-1,1):
        m.prism([(10.9,s*2,65),(10.4,s*9.8,66),(10.6,s*9,61.8),(11.1,s*2.5,62)],.5,5)
        m.prism([(11.2,s*2.8,64.5),(10.8,s*9,65),(11,s*8.5,62.5),(11.3,s*3,62)],.25,6)
        m.ellipsoid((11.6,s*5.5,63.3),(.5,1.5,1.65),2,12,6)
        m.ellipsoid((12,s*5.5,63.3),(.25,.65,1.25),5,10,4)
        m.ellipsoid((12.25,s*5.1,64),(.15,.4,.4),6,8,4)
        m.prism([(11.6,s*2.5,67),(10.5,s*9.5,68),(10.6,s*10,66.9),(11.7,s*2.6,66.1)],.4,5)
        m.ellipsoid((-1,s*13,60),(3,1.9,4.3),3,12,6)
    m.prism([(11,-1.2,63),(11,1.2,63),(15,0,58.5)],2,3)
    m.prism([(11,-2.7,54.5),(11,2.7,54.5),(11,2.2,54.1),(11,-2.2,54.1)],.1,5)
    return m

def hair(hero):
    m=Mesh()
    m.rings([(63,10,13,-4,0),(72,12,14,-3,0),(79,10,12,-3,0),(83,5,7,-4,0),(84,.5,.5,-4,0)],5,20)
    # Asymmetric tapered locks; silhouette follows sculpted wedges rather than cuboid spikes.
    for s in (-1,1):
        for j in range(4):
            y=s*(3+j*3.5); z=78+j*.9
            tip=(9-j*1.8,y+s*3,69+j*1.5)
            m.prism([(7,y-2*s,z),(-1,y+3*s,z+4),tip],3.5,5)
        m.prism([(-3,s*11,76),(-10,s*12,72),(-7,s*15,56 if hero==2 else 60),(0,s*13,64)],3,5)
    for j in range(5):
        y=(j-2)*4
        m.prism([(-7,y-3,79),(-13,y,85-abs(j-2)*1.5),(-1,y+3,79)],4,5)
    if hero==1:
        m.prism([(-12,-6,76),(-19,-6,68),(-20,-3,61),(-16,4,61),(-12,5,70)],4,5)
    if hero==0:
        m.prism([(10,-13,72),(11,13,72),(10,13,75),(10,-13,75)],2,2)
    return m

def arm(hero,side):
    m=Mesh()
    m.rings([(-43,5,5,1,0),(-35,6,6,0,0),(-25,7,7,0,0),(-20,6.5,7,0,0),(-10,7,9,0,0),(-2,8,9,0,0),(3,7,8,0,0)],3 if hero==1 else (6 if hero==2 else 0))
    m.rings([(-35,7.5,8,0,0),(-28,8.5,9,0,0),(-22,8,8,0,0)],2 if hero==1 else 1)
    m.rings([(-41,5.8,6,1,0),(-36,6,6,0,0)],6)
    m.ellipsoid((2,0,-46),(5.5,5.5,7),3,12,6)
    # Finger group lobes and thumb remove the old rectangular hand silhouette.
    for j in range(3):m.ellipsoid((5,(j-1)*2.5,-49),(2.7,1.5,3.5),3,10,5)
    m.ellipsoid((5,-side*5,-44),(3,2,4),3,10,5)
    if hero==1:
        m.ellipsoid((5,0,-42),(8,9,9),1,16,8)
        m.prism([(12,-6,-37),(13,6,-37),(13,6,-46),(12,-6,-46)],2,2)
    else:
        m.rings([(-7,8,10,0,0),(1,9,10,0,0),(5,7,8,0,0)],2 if hero==0 else 6)
    return m

def leg(hero):
    m=Mesh()
    m.rings([(-45,5,6,0,0),(-36,6,7,0,0),(-25,7,8,0,0),(-19,7,8,-1,0),(-8,8,9,0,0),(4,8,9,0,0)],5)
    m.rings([(-47,6,7,0,0),(-39,7,8,0,0),(-30,6.5,7,0,0)],0 if hero==0 else (6 if hero==2 else 2))
    m.rings([(-49,13,7,6,0),(-45,15,8,7,0),(-40,11,7,4,0),(-38,6,6,0,0)],1)
    m.prism([(6,-5,-26),(9,0,-20),(6,5,-26),(8,0,-32)],2,2)
    m.rings([(-49,13.5,7.5,6,0),(-47,13.5,7.5,6,0)],6)
    return m

bpy.context.scene.unit_settings.system='METRIC'
bpy.context.scene.unit_settings.scale_length=.01
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
MATERIALS=[]
for name,color in zip(SLOTS,COLORS):
    mat=bpy.data.materials.new(name);mat.diffuse_color=color;MATERIALS.append(mat)
OUT.mkdir(parents=True,exist_ok=True)
records=[]
for hero in range(3):
    for part,mesh in [('Body',body(hero)),('Head',head(hero)),('Hair',hair(hero)),('LeftArm',arm(hero,-1)),('RightArm',arm(hero,1)),('LeftLeg',leg(hero)),('RightLeg',leg(hero))]:
        if part in ('Head','Hair'):
            mesh.v=[(x*.85,y*.85,47+(z-47)*.78) for x,y,z in mesh.v]
        records.append(mesh.export(f'SM_AlphaHero{hero}_{part}'))
(OUT/'manifest.json').write_text(json.dumps({'revision':1,'generator':'Scripts/generate_character_meshes.py','coordinates':'centimeters, +X facing, +Z up, limb-local pivots','material_slots':SLOTS,'meshes':records},indent=2)+'\n')
print('Generated',len(records),'original character meshes;',sum(r['triangles'] for r in records),'triangles total')

if '--preview' in sys.argv:
    from mathutils import Vector
    # Assemble the exported FBX files themselves to catch export axis and scale errors.
    for hero in range(3):
        for part in ('Body','Head','Hair','LeftArm','RightArm','LeftLeg','RightLeg'):
            bpy.ops.import_scene.fbx(filepath=str(OUT/f'SM_AlphaHero{hero}_{part}.fbx'),use_custom_normals=True,use_manual_orientation=True,axis_forward='Y',axis_up='Z',global_scale=1.)
            obj=bpy.context.selected_objects[0]
            # Apply the FBX object transform before assembling source-centimeter pivots.
            assert max(abs(value-1.) for value in obj.scale)<.001, (part,obj.scale[:])
            bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
            expected=next(record for record in records if record['name']==obj.name.split('.')[0])
            observed=[[min(v.co[i] for v in obj.data.vertices) for i in range(3)],[max(v.co[i] for v in obj.data.vertices) for i in range(3)]]
            assert max(abs(a-b) for aa,bb in zip(expected['bounds_cm'],observed) for a,b in zip(aa,bb)) < .001, (part,expected['bounds_cm'],observed)
            assert max(abs(a) for a in obj.rotation_euler) < .001, (part,obj.rotation_euler[:])
            pivot={'LeftArm':(0,-24,34),'RightArm':(0,24,34),'LeftLeg':(0,-12,-29),'RightLeg':(0,12,-29)}.get(part,(0,0,0))
            obj.location=(pivot[0],pivot[1]+hero*110,pivot[2])
            for slot in obj.material_slots:
                base=slot.material.name.split('.')[0] if slot.material else 'Cloth'
                mat=slot.material.copy() if slot.material else bpy.data.materials.new(base)
                accent=((.65,.035,.05,1),(.025,.44,.36,1),(.78,.52,.14,1))[hero]
                if base=='Cloth':mat.diffuse_color=accent
                elif base=='Accent':mat.diffuse_color=accent
                elif base=='Ink' and part=='Hair':mat.diffuse_color=((.035,.045,.09,1),(.025,.13,.14,1),(.7,.51,.31,1))[hero]
                slot.material=mat
    scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH'
    scene.display.shading.light='STUDIO';scene.display.shading.studiolight_rotate_z=0.3
    scene.display.shading.color_type='MATERIAL';scene.display.shading.show_shadows=True
    scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH'
    scene.display.shading.background_type='WORLD';scene.world.color=(.7,.7,.7)
    bpy.ops.object.camera_add(location=(500,-5,70));camera=bpy.context.object
    camera.rotation_euler=(Vector((0,110,0))-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO';camera.data.ortho_scale=380;scene.camera=camera
    scene.render.resolution_x=1500;scene.render.resolution_y=850;scene.render.resolution_percentage=100
    target=ROOT/'Artifacts/Characters';target.mkdir(parents=True,exist_ok=True)
    scene.render.filepath=str(target/'ModularHeroesPreview.png');bpy.ops.render.render(write_still=True)
    import shutil
    references=OUT/'References';references.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(target/'ModularHeroesPreview.png',references/'ModeledHeroesPreview.png')
    (references/'model_preview.json').write_text(json.dumps({'generator':'Scripts/generate_character_meshes.py -- --preview','note':'Offline Blender Workbench render of reimported FBX meshes, not an Unreal screenshot. Export/reimport coordinates and bounds matched within 0.001cm; UE import/render remains unverified.'},indent=2)+'\n')
