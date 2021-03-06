#==================================================================================================#
#                                                                                                  #
#  Copyright 2012 MaidSafe.net limited                                                             #
#                                                                                                  #
#  This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or    #
#  later, and The General Public License (GPL), version 3. By contributing code to this project    #
#  You agree to the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in    #
#  the root directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also    #
#  available at:                                                                                   #
#                                                                                                  #
#    http://www.novinet.com/license                                                                #
#                                                                                                  #
#  Unless required by applicable law or agreed to in writing, software distributed under the       #
#  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,       #
#  either express or implied. See the License for the specific language governing permissions      #
#  and limitations under the License.                                                              #
#                                                                                                  #
#==================================================================================================#


SET(CTEST_PROJECT_NAME "routing")
SET(CTEST_NIGHTLY_START_TIME "00:00:00 UTC")
SET(CTEST_DROP_METHOD "http")
SET(CTEST_DROP_SITE "dash.maidsafe.net")
SET(CTEST_DROP_LOCATION "/submit.php?project=MaidSafe-Routing")
SET(CTEST_DROP_SITE_CDASH TRUE)
IF(CMAKE_CL_64)
  SET(BUILDNAME Win-x64-MSBuild)
ENDIF()
