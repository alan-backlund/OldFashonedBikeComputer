import cadquery as cq

import MountingPlate

w = 32.4
l = 43.4+2
h = 6.0
t = 2.0

insetRad = 3.8/2

# cut hole for screen into shell
lcd_screenw = 23.04 + 1
lcd_screenl = lcd_screenw
lcd_pkgw = 26.6 + 0.8
lcd_pkgl = 30.3 + 0.8

def VAdd(a, b):
    return tuple(map(sum, zip(a, b)))

tr = (-32/2, 43/2, 0)
mount1 = VAdd(tr, (16, -40.25, 0))
mount2 = VAdd(tr, (3, -3, 0))
mount3 = VAdd(tr, (29, -3, 0))
sensor1 = VAdd(tr, (29.5, -7.5, 0))
sensor2 = VAdd(tr, (7, -30.75, 0))
sensor3 = VAdd(tr, (25, -30.75, 0))

def PCB():
    pcb = cq.Workplane("XY" ).box(32, 43, 1.8).edges("|Z").fillet(2.0)
    mh = cq.Workplane("XY" ).cylinder(1.8, 2.2/2)
    pcb = pcb - mh.translate(mount1)
    pcb = pcb - mh.translate(mount2)
    pcb = pcb - mh.translate(mount3)
    sph = cq.Workplane("XY" ).cylinder(1.8, 1.0/2)
    pcb = pcb - sph.translate(sensor1)
    pcb = pcb - sph.translate(sensor2)
    pcb = pcb - sph.translate(sensor3)
    return pcb

def Top():
    # create 2mm thick shell, (w,l,h) are inside dimensions
    #top = cq.Workplane("XY" ).box(w, l, h).faces("-Z").shell(t)
    top = (cq.Workplane("XY" ).box(w+2*t, l+2*t, h+t+1)
           .translate((0, 0, t))
           .edges(">Z or |Z").fillet(2.0) )
    top = top - cq.Workplane("XY" ).box(w, l, h)
    
    #cut lip into shell
    lip = cq.Workplane("XY" ).box(w+2, l+2, 2)
    top = top - lip.translate((0, 0, -h/2+2/2))

    # lcd screen
    lcd = (cq.Workplane("XY" ).box(lcd_screenw, lcd_screenl, 4)
           .translate((0, lcd_pkgl/2-lcd_screenl/2-1.78-2, 4/2)) )
    screenInset = ( cq.Workplane("XY" ).box(lcd_pkgw, lcd_pkgl, 4)
                   .translate((0, -2, -4/2)) )
    lcd = lcd + screenInset
    screenInsetExt = ( cq.Workplane("XY" ).box(12, 10, 4)
                 .translate((0, -30.3/2-10/2, -4/2)) )
    lcd = lcd + screenInsetExt
    
    top = top - lcd.translate((0, l/2-23.04/2-(w-23.04)/2-2, h-2))
    
    # screen protector inset
    scpt = (cq.Workplane("XY" ).box(lcd_screenw+4, lcd_screenl+4, 1)
            .translate((0, lcd_pkgl/2-lcd_screenl/2-1.78+l/2-23.04/2-(w-23.04)/2-4,
                        3+2+1)) )
    top = top - scpt
    
    # cut holes for buttons into shell
    button = cq.Workplane("XY" ).box(6.4, 6.4, 4)
    button = button + cq.Workplane("XY" ).cylinder(6, 4/2).translate((0, 0, 1))
    button = button.translate((0, -l/2+6, h/2))
    top = top - button.translate((-w/2+4, 0, 0))
    top = top - button.translate((w/2-4, 0, 0))
    
    # add block with mounting hole
    mount = cq.Workplane("XY" ).box(6.0, 6.0, 3)
    insetHole = cq.Workplane("XY" ).cylinder(3, insetRad)
    #mount = mount.cut(insetHole)
    mhp = (0, -l/2+2.75, h/2-1/2)
    log("mhp {}".format(mhp))
    top = top + mount.translate(mhp)
    top = top - insetHole.translate(mhp)
    
    # add pcb support blocks
    support = cq.Workplane("XY" ).box(6.0, 5.0, 2)
    #support = support.cut(insetHole)
    tln = (-w/2, l/2, h/2)
    top = top + support.translate(VAdd(tln, (3, -2, -1)))
    top = top - insetHole.translate(VAdd(tln, (3, -2, -0.5)))
    trn = (w/2, l/2, h/2)
    top = top + support.translate(VAdd(trn, (-3, -2, -1)))
    top = top - insetHole.translate(VAdd(trn, (-3, -2, -0.5)))
    
    # cut out lip for battery access
    cutout = cq.Workplane("XY" ).box(22.0, 2.0, 2)
    top = top - cutout.translate((0, l/2+2, -h/2+1))
    
    top = top #+ PCB().translate((0, -1, 0))
    
    return top;


def Bottom():
    bottom = (cq.Workplane("XY" ).box(w+2*t, l+2*t, 1.0+t)
              .edges("|Z").fillet(2.0)
              .translate((0, 0, (1+t)/2)) )
    lip = cq.Workplane("XY" ).box(w+2.0-0.4, l+2.0-0.4, 2-0.5).translate((0, 0, (2-0.25)+t))
    cutout = cq.Workplane("XY" ).box(22.0-0.4, 2.0, 2-0.5)
    lip = lip + cutout.translate((0, l/2+1, (2-0.5)/2+3))
    bottom = bottom + lip - cq.Workplane("XY" ).box(w, l, 4.0).translate((0, 0, 4/2+t))
    second = (cq.Workplane("XY" ).box(28.4, 35.0-8, 3.4)
           .translate((0, (l+t-(35-t))/2+7/2-4, -3.5/2+0.05)) )
    # attacxh mount plate
    mountCenter = (0, l/2-34/2-5-2+0.3, -1.7)
    bottom = (bottom + MountingPlate.MountingPlate()
              .rotate((0, 0, 0), (0, 0, 1), -90)
              .translate(mountCenter) )
    #cut cavity in second
    bottom = (bottom + second
              - cq.Workplane("XY" ).box(28.4-2*t, 35-2*t-7, 3.5)
              .translate((0, (l+t-(35-t))/2+7/2-4, t-3.5/2)) )
    # add pcb supports
    support = cq.Workplane("XY" ).box(5, 5.6, 2.4)
    bottom = bottom + support.translate(VAdd(mount1, (0, -1.15, 3.3)))
    support = cq.Workplane("XY" ).box(3, 5, 2.4)
    bottom = bottom + support.translate(VAdd(sensor1, (1.5, -4.8, 3.3)))
    sensor1left = (-sensor1[0], sensor1[1], sensor1[2])
    bottom = bottom + support.translate(VAdd(sensor1left, (-1.5, -4.8, 3.3)))
    # cut pin holes
    piHole = cq.Workplane("XY" ).cylinder(12.0, 2.0/2)
    pcbb = (0, -1, -1.7)
    spoc = VAdd(sensor1, pcbb)
    bottom = bottom - piHole.translate(spoc)
    spoc = VAdd(sensor2, pcbb)
    bottom = bottom - piHole.translate(spoc)
    spoc = VAdd(sensor3, pcbb)
    bottom = bottom - piHole.translate(spoc)
    # cut mounting holes
    mHole = cq.Workplane("XY" ).cylinder(12.0, 2.4/2).translate((0, 0, 1))
    mHole = mHole + cq.Workplane("XY" ).cylinder(6.0, 4.0/2).translate((0, 0, -1.3))
    bottom = bottom - mHole.translate(VAdd(mount1, pcbb))
    tln = (0, l/2, -1.7)
    bottom = bottom - mHole.translate(VAdd(tln, (3-w/2, -2, 0)))
    bottom = bottom - mHole.translate(VAdd(tln, (w/2-3, -2, 0)))
    
    bottom = bottom #+ PCB().translate((0, -1, t+2.5+1.8/2))

    return bottom

# publish
show_object(Top() + Bottom().translate((w*1.2, 0, 0)))
cq.exporters.export(Top(),'caseTopV1.stl')
cq.exporters.export(Top(),'caseTopV1.dxf')
cq.exporters.export(Bottom(),'caseBottomV1.stl')

