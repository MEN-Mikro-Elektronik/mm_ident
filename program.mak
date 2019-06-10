#***************************  M a k e f i l e  *******************************
#
#    Description: Makefile for mm_ident
#
#-----------------------------------------------------------------------------
#   Copyright (c) 2019, MEN Mikro Elektronik GmbH
#*****************************************************************************
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

MAK_NAME=mm_ident
# the next line is updated during the MDIS installation
STAMPED_REVISION="7f98380_2019-06-07"

DEF_REVISION=MAK_REVISION=$(STAMPED_REVISION)
MAK_SWITCH=$(SW_PREFIX)$(DEF_REVISION)

MAK_INP1=mm_ident$(INP_SUFFIX)

MAK_INP=$(MAK_INP1)
