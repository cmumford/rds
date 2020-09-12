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

#include "rds_spy_log_reader.h"

#include <stdlib.h>
#include <string.h>

#include <memory>

#include <rds_decoder.h>

namespace {

/**
 * Parse a block string into a rds_block.
 *
 * The text is four hex characters or the special string "----" indicating
 * a missing block.
 *
 * @param text The input string.
 *
 * @return The parsed block.
 */
struct rds_block ParseBlock(const char* text) {
  struct rds_block block;

  if (strcmp(text, "----") == 0) {
    block.val = 0;
    block.errors = BLER_6_PLUS;
  } else {
    block.val = strtol(text, nullptr, 16);
    block.errors = BLER_NONE;
  }
  return block;
}

/**
 * Trim the whitespace on the end of the string.
 */
void rtrim(char* str) {
  char* end = str + strlen(str) - 1;
  while (str <= end && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';
}

bool appears_to_be_block_line(const char* line) {
  // F202 2410 4652 414E @2019/05/04 02:29:17.94
  // F202 2410 4652 414E @2019/05/04 02:29:17.940
  if (strlen(line) < 22)
    return false;

  if (line[4] != ' ')
    return false;
  if (line[9] != ' ')
    return false;
  if (line[14] != ' ')
    return false;
  if (line[19] != ' ')
    return false;
  if (line[20] != '@')
    return false;
  return true;
}

}  // namespace

//
// This is a very simple implementation of this file read operation. It is
// *not* tolerant of file formatting errors.
//
bool LoadRdsSpyFile(const std::string& path,
                    std::vector<struct rds_blocks>* blocks) {
  FILE* f = fopen(path.c_str(), "r");
  if (f == nullptr) {
    perror("Error reading rds-spy file.");
    return false;
  }

  char line[120];
  while (fgets(line, ARRAY_SIZE(line), f) != nullptr) {
    line[ARRAY_SIZE(line) - 1] = '\0';
    rtrim(line);
    if (appears_to_be_block_line(line)) {
      struct rds_blocks blk;

      char hex[5];
      hex[4] = '\0';

      memcpy(hex, line + 0, 4);
      blk.a = ParseBlock(hex);

      memcpy(hex, line + 5, 4);
      blk.b = ParseBlock(hex);

      memcpy(hex, line + 10, 4);
      blk.c = ParseBlock(hex);

      memcpy(hex, line + 15, 4);
      blk.d = ParseBlock(hex);

      blocks->push_back(std::move(blk));
    }
  }

  fclose(f);

  return true;
}
