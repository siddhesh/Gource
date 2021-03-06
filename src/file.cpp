/*
    Copyright (C) 2009 Andrew Caudwell (acaudwell@gmail.com)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version
    3 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "file.h"

float gGourceFileDiameter  = 8.0;

std::vector<RFile*> gGourceRemovedFiles;

RFile::RFile(const std::string & name, const vec3f & colour, const vec2f & pos, int tagid) : Pawn(name,pos,tagid) {
    hidden = true;
    size = gGourceFileDiameter;
    radius = size * 0.5;

    setGraphic(gGourceSettings.file_graphic);

    speed = 5.0;
    nametime = 4.0;
    name_interval = nametime;

    namecol     = vec3f(1.0, 1.0, 1.0);
    file_colour = colour;

    last_action = 0.0;
    expiring=false;
    removing=false;

    shadow = true;

    distance = 0;

    setFilename(name);

    namelist = glGenLists(1);

    font = 0;
    setSelected(false);

    dir = 0;
}

RFile::~RFile() {
    glDeleteLists(namelist, 1);
}

void RFile::remove(bool force) {
    last_action = elapsed - gGourceSettings.file_idle_time;
    if(force) removing = true;
}

void RFile::setDir(RDirNode* dir) {
    this->dir = dir;
}

RDirNode* RFile::getDir() const{
    return dir;
}

vec2f RFile::getAbsolutePos() const{
    return pos + dir->getPos();
}

void RFile::setFilename(const std::string& abs_file_path) {
    
    fullpath = abs_file_path;

    size_t pos = fullpath.rfind('/');

    if(pos != std::string::npos) {
        path = name.substr(0,pos+1);
        name = name.substr(pos+1, std::string::npos);
    } else {
        path = std::string("");
        name = abs_file_path;
    }

    //trim name to just extension
    int dotsep=0;

    if((dotsep=name.rfind(".")) != std::string::npos && dotsep != name.size()-1 && dotsep != 0) {
        ext = name.substr(dotsep+1);
    }
}

int call_count = 0;


void RFile::setSelected(bool selected) {
    if(font.getFTFont()!=0 && this->selected==selected) return;

    if(selected) {
        font = fontmanager.grab("FreeSans.ttf", 18);
    } else {
        font = fontmanager.grab("FreeSans.ttf", 14);
    }

    font.dropShadow(false);
    font.roundCoordinates(true);

    Pawn::setSelected(selected);

    //pre-compile name display list
    //glNewList(namelist, GL_COMPILE);
    //   font.draw(0.0f, 0.0f, (selected || shortname.size()==0) ? name : shortname);
    //glEndList();
}

void RFile::colourize() {
    file_colour = ext.size() ? colourHash(ext) : vec3f(1.0f, 1.0f, 1.0f);
}

const vec3f& RFile::getNameColour() const{
    return selected ? selectedcol : namecol;
}

const vec3f & RFile::getFileColour() const{
    return file_colour;
}

vec3f RFile::getColour() const{
    if(selected) return vec3f(1.0, 1.0, 1.0);

    float lc = elapsed - last_action;

    if(lc<1.0) {
        return touch_colour * (1.0-lc) + file_colour * lc;
    }

    return file_colour;
}

float RFile::getAlpha() const{
    float alpha = Pawn::getAlpha();

    //user fades out if not doing anything
    if(elapsed - last_action > gGourceSettings.file_idle_time) {
        alpha = 1.0 - std::min(elapsed - last_action - gGourceSettings.file_idle_time, 1.0f);
    }

    return alpha;
}

void RFile::logic(float dt) {
    Pawn::logic(dt);

    vec2f dest_pos = dest;
/*
    if(dir->getParent() != 0 && dir->noDirs()) {
        vec2f dirnorm = dir->getNodeNormal();
        dest_pos = dirnorm + dest;
    }*/

    float dradius = dir->getRadius();

    dest_pos = dest_pos * distance;

    accel = dest_pos - pos;

    // apply accel
    vec2f accel2 = accel * speed * dt;

    if(accel2.length2() > accel.length2()) {
        accel2 = accel;
    }

    pos += accel2;

    //files have no momentum
    accel = vec2f(0.0f, 0.0f);

    // has completely faded out
    if(!expiring && elapsed - last_action >= gGourceSettings.file_idle_time + 1.0) {
        expiring=true;

        bool found = false;

        for(std::vector<RFile*>::iterator it = gGourceRemovedFiles.begin(); it != gGourceRemovedFiles.end(); it++) {
            if((*it) == this) {
                found = true;
                break;
            }

        }

        if(!found) {
            gGourceRemovedFiles.push_back(this);
            //fprintf(stderr, "expiring %s\n", fullpath.c_str());
        }
    }

    if(isHidden() && !removing) elapsed = 0.0;
}

void RFile::touch(const vec3f & colour) {
    if(removing) return;

    //fprintf(stderr, "touch %s\n", fullpath.c_str());

    last_action = elapsed;
    touch_colour = colour;

    //un expire file
    if(expiring) {
        for(std::vector<RFile*>::iterator it = gGourceRemovedFiles.begin(); it != gGourceRemovedFiles.end(); it++) {
            if((*it) == this) {
                gGourceRemovedFiles.erase(it);
                break;
            }
        }

        expiring=false;
    }

    showName();
    setHidden(false);
    dir->fileUpdated(true);
}

void RFile::setHidden(bool hidden) {
    if(this->hidden==true && hidden==false && dir !=0) {
        dir->addVisible();
    }

    Pawn::setHidden(hidden);
}

void RFile::drawNameText(float alpha) const {

    if(!selected && alpha <= 0.01) return;

    vec3f nameCol    = getNameColour();
    float name_alpha = selected ? 1.0 : alpha;

    vec3f drawpos = screenpos;

    //drawpos.x += 10.0;
    //drawpos.y -= 10.0;

    bool show_file_ext = gGourceSettings.file_extensions;
    
    glPushMatrix();

        glTranslatef(drawpos.x, drawpos.y, 0.0);

        //hard coded drop shadow
        glPushMatrix();
            glTranslatef(1.0, 1.0, 0.0);
            glColor4f(0.0, 0.0, 0.0, name_alpha * 0.7f);
            //glCallList(namelist);
            font.draw(0.0f, 0.0f, (selected || !show_file_ext) ? name : ext);
        glPopMatrix();

        //draw name
        glColor4f(nameCol.x, nameCol.y, nameCol.z, name_alpha);
        //glCallList(namelist);
        font.draw(0.0f, 0.0f, (selected || !show_file_ext) ? name : ext);

    glPopMatrix();
}

void RFile::draw(float dt) {
    Pawn::draw(dt);

    glLoadName(0);
}
