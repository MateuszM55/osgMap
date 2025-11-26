import shapefile
import numpy as np
import matplotlib.pyplot as plt



def get_first_nbr_from_str(input_str):
    #print ("input str : " + input_str)
    '''
    :param input_str: strings that contains digit and words
    :return: the number extracted from the input_str
    demo:
    'ab324.23.123xyz': 324.23
    '.5abc44': 0.5
    '''
    if not input_str and not isinstance(input_str, str):
        return 0
    out_number = ''
    for ele in input_str:
        if (ele == '.' and '.' not in out_number) or ele.isdigit():
            out_number += ele
        elif out_number:
            break
    if not out_number:
        return 0.0
    #print ("get_first_nbr_from_str number : " + out_number)
    res=0.0
    try:
        res = float(out_number)
    except ValueError:
        print ("input str not valid : " + input_str)
        return 0.0
    return res


"""
 IMPORT THE SHAPEFILE
"""
shp_file_bld='buildings.shp'
dat_dir='./'
sfb = shapefile.Reader(dat_dir+shp_file_bld)

print ('number of shapes imported:',len(sfb.shapes()))
print (' ')
print ('geometry attributes in each shape:')
for name in dir(sfb.shape()):
    if not name.startswith('__'):
       print (name)

fld_bld = sfb.fields[1:]
field_bld_names = [field[0] for field in fld_bld]
fld_bld_name0='other_tags'
fld_bld_ndx0=field_bld_names.index(fld_bld_name0)


file_city_name='buildings_levels'
w = shapefile.Writer(dat_dir + file_city_name)
w.field('id', 'N')
w.field('height', 'N')
shapeid=0
for shaperec in sfb.iterShapeRecords():
    bld = shaperec.shape
    bld_rec=sfb.record(shapeid)
    bld_tags=bld_rec[fld_bld_ndx0]
    bld_tags_list=bld_tags.split(',')
    bld_height=0.0
    added=0
    for tag in bld_tags_list:
        lvl_ind=tag.find("levels")
        if ( lvl_ind > 0 ):
            key_val_pair=tag.split('=>')
            if(len(key_val_pair)<2): break
            bld_height = 2.7 * get_first_nbr_from_str(key_val_pair[1]) * 100
            if (bld_height==0.0):
                break
            added=1
    if added==0 :
        for tag in bld_tags_list:
            hgh_ind=tag.find("height")
            if ( hgh_ind > 0 ):
                key_val_pair=tag.split('=>')
                if(len(key_val_pair)<2 or key_val_pair[0]!="height"): continue
                bld_height = get_first_nbr_from_str(key_val_pair[1]) * 100
                ft_ind=key_val_pair[1].find("ft")
                if ( ft_ind > 0 ):
                    bld_height = bld_height / 3.28
                if (bld_height==0.0):
                    continue
                added=1
    if added==0 :
        bld_height = 270
        added=1
    if added==1 :
        w.record(id=shapeid, height=bld_height)
        w.shape(bld)
    shapeid+=1
print("save file : " + dat_dir + file_city_name)
w.close()





