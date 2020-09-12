/**
 * @file
 *
 * @author Chris Mumford
 *
 * @license
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <string>
#include <vector>

#include <stdbool.h>

#include <rds_decoder.h>

/**
 * Read in the contents of a RDS Spy data file, and populate a vector of
 * rds_blocks with those contents.
 *
 * @param path   The path to the RDS Spy data file.
 * @param blocks The vector of blocks to be populated. New blocks will be
 *               pushed to the back of this vector.
 *
 * @return true if successful.
 */
bool LoadRdsSpyFile(const std::string& path,
                    std::vector<struct rds_blocks>* blocks);
